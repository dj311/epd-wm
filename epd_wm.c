/*
 * epd-wm: a Wayland window manager for IT8951 E-Paper displays
 *
 * Copyright (C) 2019-2020 Daniel Jones
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>

#include "wm/idle_inhibit_v1.h"
#include "wm/seat.h"
#include "wm/server.h"
#include "wm/view.h"
#include "wm/xdg_shell.h"
#include "wm/xwayland.h"

#include "epd/epd_backend.h"
#include "epd/epd_output.h"

#include "hacks/wlr_backend_multi.h"

static bool
spawn_primary_client(
  char *argv[],
  pid_t * pid_out
)
{
  pid_t pid = fork();
  if (pid == 0) {
    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
    execvp(argv[0], argv);
    _exit(1);
  } else if (pid == -1) {
    wlr_log_errno(WLR_ERROR, "Unable to fork");
    return false;
  }

  *pid_out = pid;
  wlr_log(WLR_DEBUG, "Child process created with pid %d", pid);
  return true;
}

static bool
drop_permissions(
  void
)
{
  if (getuid() != geteuid() || getgid() != getegid()) {
    if (setuid(getuid()) != 0 || setgid(getgid()) != 0) {
      wlr_log(WLR_ERROR, "Unable to drop root, refusing to start");
      return false;
    }
  }

  if (setuid(0) != -1) {
    wlr_log(WLR_ERROR, "Unable to drop root (we shouldn't be able to "
            "restore it after setuid), refusing to start");
    return false;
  }

  return true;
}

static int
handle_signal(
  int signal,
  void *data
)
{
  struct wl_display *display = data;

  switch (signal) {
  case SIGINT:
    /* Fallthrough */
  case SIGTERM:
    wl_display_terminate(display);
    return 0;
  default:
    return 1;
  }
}

static void
usage(
  FILE * file,
  const char *cage
)
{
  fprintf(file, "Usage: %s [OPTIONS] [--] APPLICATION\n"
          "\n"
          " -d\t Don't draw client side decorations, when possible\n"
          " -r\t Rotate the output 90 degrees clockwise, specify up to three times\n"
#ifdef DEBUG
          " -D\t Turn on damage tracking debugging\n"
#endif
          " -h\t Display this help message\n"
          "\n"
          " Use -- when you want to pass arguments to APPLICATION\n", cage);
}

static bool
parse_args(
  struct cg_server *server,
  int argc,
  char *argv[]
)
{
  int c;
#ifdef DEBUG
  while ((c = getopt(argc, argv, "drDh")) != -1) {
#else
  while ((c = getopt(argc, argv, "drh")) != -1) {
#endif
    switch (c) {
    case 'd':
      server->xdg_decoration = true;
      break;
    case 'r':
      server->output_transform++;
      if (server->output_transform > WL_OUTPUT_TRANSFORM_270) {
        server->output_transform = WL_OUTPUT_TRANSFORM_NORMAL;
      }
      break;
#ifdef DEBUG
    case 'D':
      server->debug_damage_tracking = true;
      break;
#endif
    case 'h':
      usage(stdout, argv[0]);
      return false;
    default:
      usage(stderr, argv[0]);
      return false;
    }
  }

  if (optind >= argc) {
    usage(stderr, argv[0]);
    return false;
  }

  return true;
}

int
main(
  int argc,
  char *argv[]
)
{
  /* cg_server stores /our/ state */
  struct cg_server server = { 0 };

  struct wl_event_loop *event_loop = NULL;
  struct wl_event_source *sigint_source = NULL;
  struct wl_event_source *sigterm_source = NULL;
  struct wlr_renderer *renderer = NULL;
  struct wlr_compositor *compositor = NULL;
  struct wlr_data_device_manager *data_device_mgr = NULL;
  struct wlr_server_decoration_manager *server_decoration_manager = NULL;
  struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
  struct wlr_xdg_shell *xdg_shell = NULL;
  struct wlr_xwayland *xwayland = NULL;
  struct wlr_xcursor_manager *xcursor_manager = NULL;

  int ret = 0;

  /* 1. Parse and validate arguments */

  if (!parse_args(&server, argc, argv)) {
    return 1;
  }
#ifdef DEBUG
  wlr_log_init(WLR_DEBUG, NULL);
#else
  wlr_log_init(WLR_ERROR, NULL);
#endif

  /* 2. Create a Wayland display object.

     What is a display object?

     It's /not/ what I assumed. There's a chapter in the Wayland book
     on it - https://wayland-book.com/wayland-display.html.

     My understanding is that the display "manages joint ownership
     over objects between the client and the server". It's about a
     connection with the server, not a particular piece of
     hardware. Conceptually like an x11 session, I believe.

     So the display object is how we manage communication and resource
     sharing with clients.

   */

  server.wl_display = wl_display_create();
  if (!server.wl_display) {
    wlr_log(WLR_ERROR, "Cannot allocate a Wayland display");
    return 1;
  }

  /* 3. Initialise the event loop.

     (and also register for interrupt and termination signals that are
     sent to this process).
   */

  event_loop = wl_display_get_event_loop(server.wl_display);
  sigint_source =
    wl_event_loop_add_signal(event_loop, SIGINT, handle_signal,
                             &server.wl_display);
  sigterm_source =
    wl_event_loop_add_signal(event_loop, SIGTERM, handle_signal,
                             &server.wl_display);

  /* 3. Initialise a backend by asking wlroots to give us the right
     one.

     What is a backend?
     TODO

   */

  // This doesn't support embedding within existing Wayland or X11
  // sessions. This implementation expects to be able to take control
  // of the current user session, it's inputs and it's outputs.
  struct wlr_backend *backend = wlr_multi_backend_create(server.wl_display);
  struct wlr_multi_backend *multi = (struct wlr_multi_backend *) backend;
  if (!backend) {
    wlr_log(WLR_ERROR, "could not allocate multibackend");
    ret = 1;
    goto end;
  }

  wlr_log(WLR_INFO, "Adding a session to multi-backend");
  multi->session = wlr_session_create(server.wl_display);
  if (!multi->session) {
    wlr_log(WLR_ERROR, "Failed to start a DRM session");
    wlr_backend_destroy(backend);
    ret = 1;
    goto end;
  }
  wlr_log(WLR_INFO, "Adding a session to multi-backend: success");

  wlr_log(WLR_INFO, "Adding libinput backend to multi-backend");
  struct wlr_backend *libinput_backend =
    wlr_libinput_backend_create(server.wl_display, multi->session);
  if (!libinput_backend) {
    // We need input events: if this fails, exit.
    wlr_log(WLR_ERROR, "Failed to start libinput backend");
    wlr_session_destroy(multi->session);
    wlr_backend_destroy(backend);
    ret = 1;
    goto end;
  }
  wlr_multi_backend_add(backend, libinput_backend);
  wlr_log(WLR_INFO, "Adding libinput backend to multi-backend: success");

  wlr_log(WLR_INFO, "Adding epd backend to multi-backend");
  struct wlr_backend *epd_backend =
    epd_backend_create(server.wl_display, NULL);
  wlr_multi_backend_add(backend, epd_backend);
  wlr_log(WLR_INFO, "Adding epd backend to multi-backend: success");

  wlr_log(WLR_INFO, "Adding /dev/sg1 as output for epd");
  epd_backend_add_output(epd_backend, "/dev/sg1", 1810);
  wlr_log(WLR_INFO, "Adding /dev/sg1 as output for epd: success");

  server.backend = backend;

  /* 4. Drop root permissions. */

  wlr_log(WLR_INFO, "Drop root permissions");
  if (!drop_permissions()) {
    ret = 1;
    goto end;
  }


  /* 5. The backend allocates us a renderer, which we use to
     initialise our display.

     What is a renderer?
     TODO

   */

  wlr_log(WLR_INFO, "Link renderer to display");
  renderer = wlr_backend_get_renderer(server.backend);
  wlr_renderer_init_wl_display(renderer, server.wl_display);

  /* I believe that server.views is a cage concept (and they are using
     waylands list costruct for convenience). */
  wlr_log(WLR_INFO, "Init server views");
  wl_list_init(&server.views);

  /* The output layout is about mapping physical screens into
     space. And to define how they are laid out relative to one
     another. */
  wlr_log(WLR_INFO, "Init output layout");
  server.output_layout = wlr_output_layout_create();
  if (!server.output_layout) {
    wlr_log(WLR_ERROR, "Unable to create output layout");
    ret = 1;
    goto end;
  }

  /* This compositor doesn't seem to be used very much, I get the
     impression most of the magic is handled by wlroots. It's hooked
     up to the renderer though.
   */
  wlr_log(WLR_INFO, "Create compositor");
  compositor = wlr_compositor_create(server.wl_display, renderer);
  if (!compositor) {
    wlr_log(WLR_ERROR, "Unable to create the wlroots compositor");
    ret = 1;
    goto end;
  }

  /* TODO: What is this? */
  wlr_log(WLR_INFO, "Setup device manager");
  data_device_mgr = wlr_data_device_manager_create(server.wl_display);
  if (!data_device_mgr) {
    wlr_log(WLR_ERROR, "Unable to create the data device manager");
    ret = 1;
    goto end;
  }

  /* Configure a listener to be notified when new outputs are
   * available on the backend. We use this only to detect the
   * first output and ignore subsequent outputs. */
  /* server.new_output.notify = handle_new_output; */
  /* wl_signal_add(&server.backend->events.new_output, &server.new_output); */

  /* 6. Setup user and session style concepts. */

  server.seat = seat_create(&server);
  if (!server.seat) {
    wlr_log(WLR_ERROR, "Unable to create the seat");
    ret = 1;
    goto end;
  }

  server.idle = wlr_idle_create(server.wl_display);
  if (!server.idle) {
    wlr_log(WLR_ERROR, "Unable to create the idle tracker");
    ret = 1;
    goto end;
  }

  /* 6. Now we are setting up desktop environment/shell type stuff:
     - Users
     - Idle/suspend/...
     - XDG shell spec
     - Decorations
     - Cursor
     - XWayland
   */

  /* This is idle inhibitor prevents screensavers, suspend
     etc. Presumably useful for kiosks, but maybe not for us. */
  server.idle_inhibit_v1 = wlr_idle_inhibit_v1_create(server.wl_display);
  if (!server.idle_inhibit_v1) {
    wlr_log(WLR_ERROR, "Cannot create the idle inhibitor");
    ret = 1;
    goto end;
  }
  server.new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1_new;
  wl_signal_add(&server.idle_inhibit_v1->events.new_inhibitor,
                &server.new_idle_inhibitor_v1);
  wl_list_init(&server.inhibitors);

  /* TODO: What is this xdg shell for? My guess is that it implements
     the xdg specification for us. */
  xdg_shell = wlr_xdg_shell_create(server.wl_display);
  if (!xdg_shell) {
    wlr_log(WLR_ERROR, "Unable to create the XDG shell interface");
    ret = 1;
    goto end;
  }
  server.new_xdg_shell_surface.notify = handle_xdg_shell_surface_new;
  wl_signal_add(&xdg_shell->events.new_surface,
                &server.new_xdg_shell_surface);

  xdg_decoration_manager =
    wlr_xdg_decoration_manager_v1_create(server.wl_display);
  if (!xdg_decoration_manager) {
    wlr_log(WLR_ERROR, "Unable to create the XDG decoration manager");
    ret = 1;
    goto end;
  }
  wl_signal_add(&xdg_decoration_manager->events.new_toplevel_decoration,
                &server.xdg_toplevel_decoration);
  server.xdg_toplevel_decoration.notify = handle_xdg_toplevel_decoration;

  server_decoration_manager =
    wlr_server_decoration_manager_create(server.wl_display);
  if (!server_decoration_manager) {
    wlr_log(WLR_ERROR, "Unable to create the server decoration manager");
    ret = 1;
    goto end;
  }
  wlr_server_decoration_manager_set_default_mode(server_decoration_manager,
                                                 server.xdg_decoration ?
                                                 WLR_SERVER_DECORATION_MANAGER_MODE_SERVER
                                                 :
                                                 WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);

  /* XWayland */

  xwayland = wlr_xwayland_create(server.wl_display, compositor, true);
  if (!xwayland) {
    wlr_log(WLR_ERROR, "Cannot create XWayland server");
    ret = 1;
    goto end;
  }
  server.new_xwayland_surface.notify = handle_xwayland_surface_new;
  wl_signal_add(&xwayland->events.new_surface, &server.new_xwayland_surface);

  xcursor_manager = wlr_xcursor_manager_create(DEFAULT_XCURSOR, XCURSOR_SIZE);
  if (!xcursor_manager) {
    wlr_log(WLR_ERROR, "Cannot create XWayland XCursor manager");
    ret = 1;
    goto end;
  }

  if (setenv("DISPLAY", xwayland->display_name, true) < 0) {
    wlr_log_errno(WLR_ERROR, "Unable to set DISPLAY for XWayland.",
                  "Clients may not be able to connect");
  } else {
    wlr_log(WLR_DEBUG, "XWayland is running on display %s",
            xwayland->display_name);
  }

  if (wlr_xcursor_manager_load(xcursor_manager, 1)) {
    wlr_log(WLR_ERROR, "Cannot load XWayland XCursor theme");
  }
  struct wlr_xcursor *xcursor =
    wlr_xcursor_manager_get_xcursor(xcursor_manager, DEFAULT_XCURSOR, 1);
  if (xcursor) {
    struct wlr_xcursor_image *image = xcursor->images[0];
    wlr_xwayland_set_cursor(xwayland, image->buffer,
                            image->width * 4, image->width, image->height,
                            image->hotspot_x, image->hotspot_y);
  }

  /* 8. Now we start everything up */

  const char *socket = wl_display_add_socket_auto(server.wl_display);
  if (!socket) {
    wlr_log_errno(WLR_ERROR, "Unable to open Wayland socket");
    ret = 1;
    goto end;
  }

  if (!wlr_backend_start(server.backend)) {
    wlr_log(WLR_ERROR, "Unable to start the wlroots backend");
    ret = 1;
    goto end;
  }

  if (setenv("WAYLAND_DISPLAY", socket, true) < 0) {
    wlr_log_errno(WLR_ERROR, "Unable to set WAYLAND_DISPLAY.",
                  "Clients may not be able to connect");
  } else {
    wlr_log(WLR_DEBUG, "Cage is running on Wayland display %s", socket);
  }

  wlr_xwayland_set_seat(xwayland, server.seat->seat);

  pid_t pid;
  if (!spawn_primary_client(argv + optind, &pid)) {
    ret = 1;
    goto end;
  }

  wl_display_run(server.wl_display);

  wlr_xwayland_destroy(xwayland);
  wlr_xcursor_manager_destroy(xcursor_manager);
  wl_display_destroy_clients(server.wl_display);

  waitpid(pid, NULL, 0);

end:
  wl_event_source_remove(sigint_source);
  wl_event_source_remove(sigterm_source);
  seat_destroy(server.seat);
  wlr_server_decoration_manager_destroy(server_decoration_manager);
  wlr_xdg_decoration_manager_v1_destroy(xdg_decoration_manager);
  wlr_xdg_shell_destroy(xdg_shell);
  wlr_idle_inhibit_v1_destroy(server.idle_inhibit_v1);
  if (server.idle) {
    wlr_idle_destroy(server.idle);
  }
  wlr_data_device_manager_destroy(data_device_mgr);
  wlr_compositor_destroy(compositor);
  wlr_output_layout_destroy(server.output_layout);
  /* This function is not null-safe, but we only ever get here
     with a proper wl_display. */
  wl_display_destroy(server.wl_display);
  return ret;
}
