/*
 * Copyright (c) 2020 Daniel Jones
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 *
 * See the LICENSE file accompanying this file.
 */

#include <assert.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_input_device.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/util/log.h>
#include "glapi.h"

#include <epd/epd_driver.h>
#include <epd/epd_backend.h>
#include <epd/epd_output.h>

/* Reading summary: I don't think many changes will be needed from
   wlroots' headless backend. I reckon the bulk of the implementation
   will be in the output.

   Key question: I thought that the backend handled detecting new
   outputs. But I can't find any code for it here.

   In wlr_epd_backend_create the list of outputs is initialised as
   empty. Then, backend_start iterates over the (presumably populated)
   list of outputs. I want to know: when, where and how is that list
   populated? I need to stick in some it8951 related code where ever
   that is.
 */

bool
backend_is_epd(
  struct wlr_backend *backend
)
{
  return backend->impl == &backend_impl;
}

struct epd_backend *
epd_backend_from_backend(
  struct wlr_backend *wlr_backend
)
{
  /* This function is just a type check followed by cast. It ensures
     the wlr_backend given is a wlr_epd_backend (or fails if it
     isn't).
   */
  assert(backend_is_epd(wlr_backend));
  return (struct epd_backend *) wlr_backend;
}

static bool
backend_start(
  struct wlr_backend *wlr_backend
)
{
  /* The start up process is:
     - type check for wlr_epd_backend
     - link up events to each output
     - let others know that each of these outputs is active
     - link up events to each input
     - let others know that each of these inputs is active

     Then update our state to reflect that we've started up.
   */

  struct epd_backend *backend = epd_backend_from_backend(wlr_backend);
  wlr_log(WLR_INFO, "Starting epd backend");

  struct wlr_epd_output *output;
  wl_list_for_each(output, &backend->outputs, link) {
    wl_event_source_timer_update(output->frame_timer, output->frame_delay);
    wlr_output_update_enabled(&output->wlr_output, true);
    wlr_signal_emit_safe(&backend->backend.events.new_output,
                         &output->wlr_output);
  }

  struct wlr_epd_input_device *input_device;
  wl_list_for_each(input_device, &backend->input_devices,
                   wlr_input_device.link) {
    wlr_signal_emit_safe(&backend->backend.events.new_input,
                         &input_device->wlr_input_device);
  }

  backend->started = true;
  return true;
}

static void
backend_destroy(
  struct wlr_backend *wlr_backend
)
{
  struct epd_backend *backend = epd_backend_from_backend(wlr_backend);
  if (!wlr_backend) {
    return;
  }

  wl_list_remove(&backend->display_destroy.link);

  struct wlr_epd_output *output, *output_tmp;
  wl_list_for_each_safe(output, output_tmp, &backend->outputs, link) {
    wlr_output_destroy(&output->wlr_output);
  }

  struct wlr_epd_input_device *input_device, *input_device_tmp;
  wl_list_for_each_safe(input_device, input_device_tmp,
                        &backend->input_devices, wlr_input_device.link) {
    wlr_input_device_destroy(&input_device->wlr_input_device);
  }

  wlr_signal_emit_safe(&wlr_backend->events.destroy, backend);

  wlr_renderer_destroy(backend->renderer);
  wlr_egl_finish(&backend->egl);
  free(backend);
}

static struct wlr_renderer *
backend_get_renderer(
  struct wlr_backend *wlr_backend
)
{
  struct wlr_epd_backend *backend = epd_backend_from_backend(wlr_backend);
  return backend->renderer;
}

static const struct wlr_backend_impl backend_impl = {
  .start = backend_start,
  .destroy = backend_destroy,
  .get_renderer = backend_get_renderer,
};

static void
handle_display_destroy(
  struct wl_listener *listener,
  void *data
)
{
  struct epd_backend *backend =
    wl_container_of(listener, backend, display_destroy);
  backend_destroy(&backend->backend);
}

struct wlr_backend *
epd_backend_create(
  struct wl_display *display,
  wlr_renderer_create_func_t create_renderer_func
)
{
  wlr_log(WLR_INFO, "Creating epd backend");

  /* This initial section just sets up a wlr_backend struct in memory
     and makes sure to initialise its more complex members.
   */
  struct epd_backend *backend = calloc(1, sizeof(struct epd_backend));
  if (!backend) {
    wlr_log(WLR_ERROR, "Failed to allocate epd_backend");
    return NULL;
  }
  wlr_backend_init(&backend->backend, &backend_impl);
  backend->display = display;
  wl_list_init(&backend->outputs);
  wl_list_init(&backend->input_devices);

  static const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_ALPHA_SIZE, 0,
    EGL_BLUE_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_RED_SIZE, 1,
    EGL_NONE,
  };

  if (!create_renderer_func) {
    create_renderer_func = wlr_renderer_autocreate;
  }

  backend->renderer = create_renderer_func(&backend->egl,
                                           EGL_PLATFORM_SURFACELESS_MESA,
                                           NULL, (EGLint *) config_attribs,
                                           0);
  if (!backend->renderer) {
    wlr_log(WLR_ERROR, "Failed to create renderer");
    free(backend);
    return NULL;
  }

  backend->display_destroy.notify = handle_display_destroy;
  wl_display_add_destroy_listener(display, &backend->display_destroy);

  return &backend->backend;
}

bool
wlr_backend_is_epd(
  struct wlr_backend *backend
)
{
  return backend->impl == &backend_impl;
}
