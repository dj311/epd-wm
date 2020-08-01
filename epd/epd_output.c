/*
 * Copyright (c) 2020 Daniel Jones
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 *
 * See the LICENSE file accompanying this file.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include <epd/epd_driver.h>
#include <epd/epd_backend.h>
#include <epd/epd_output.h>

#include <hacks/wlr_utils_signal.h>


/* Reading Notes:

   For more information about a wlr_output and how it works, look at
   wlr_output_impl's definition in wlroots. In the version of wlroots
   I'm linking to the interface isn't documented, but there are docs
   in the current master:
        https://github.com/swaywm/wlroots/blob/master/include/wlr/interfaces/wlr_output.h#L17

   Summary - ...

   Questions - ...

 */


static EGLSurface
egl_create_surface(
  struct wlr_egl *egl,
  unsigned int width,
  unsigned int height
)
{
  EGLint attribs[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };

  EGLSurface surf =
    eglCreatePbufferSurface(egl->display, egl->config, attribs);
  if (surf == EGL_NO_SURFACE) {
    wlr_log(WLR_ERROR, "Failed to create EGL surface");
    return EGL_NO_SURFACE;
  }
  return surf;
}

static bool
output_attach_render(
  struct wlr_output *wlr_output,
  int *buffer_age
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);
  return wlr_egl_make_current(&output->backend->egl, output->egl_surface,
                              buffer_age);
}

static bool
output_commit(
  struct wlr_output *wlr_output
)
{
  /*
     In this context, "commit" means "display on the physical screen"
     (I think). For the headless output, they don't do anything. But we
     want to with the epd.

     I should look at the drm implementation to what they do here.
   */


  // Nothing needs to be done for pbuffers
  wlr_output_send_present(wlr_output, NULL);
  return true;
}

static void
output_destroy(
  struct wlr_output *wlr_output
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);

  status = epd_reset(output->epd_device);
  close(output->epd_device.fd);
  free(output->epd_device);

  wl_list_remove(&output->link);

  wl_event_source_remove(output->frame_timer);

  wlr_egl_destroy_surface(&output->backend->egl, output->egl_surface);
  free(output);
}

static int
signal_frame(
  void *data
)
{
  /*
     What does this output do?
   */
  struct epd_output *output = data;
  wlr_output_send_frame(&output->wlr_output);
  wl_event_source_timer_update(output->frame_timer, output->frame_delay);
  return 0;
}

static const struct wlr_output_impl output_impl = {
  .destroy = output_destroy,
  .attach_render = output_attach_render,
  .commit = output_commit,
};

bool
output_is_epd(
  struct wlr_output *wlr_output
)
{
  return wlr_output->impl == &output_impl;
}

struct epd_output *
epd_output_from_output(
  struct wlr_output *wlr_output
)
{
  /* Ensure given wlr_output is a epd_output, if not, fail. */
  assert(output_is_epd(wlr_output));
  return (struct epd_output *) wlr_output;
}

unsigned int
epd_output_get_width(
  struct wlr_output *wlr_output
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);
  return ntohl(output->epd_device.info.width);
}


unsigned int
epd_output_get_height(
  struct wlr_output *wlr_output
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);
  return ntohl(output->epd_device.info.height);
}

struct wlr_output *
epd_backend_add_output(
  struct wlr_backend *wlr_backend,
  char epd_path[],
  unsigned int epd_vcom
)
{
  struct epd_backend *backend = epd_backend_from_backend(wlr_backend);

  struct epd_output *output = calloc(1, sizeof(struct epd_output));
  if (output == NULL) {
    wlr_log(WLR_ERROR, "Failed to allocate epd_output");
    return NULL;
  }

  output->backend = backend;

  wlr_output_init(&output->wlr_output, &backend->backend, &output_impl,
                  backend->display);
  struct wlr_output *wlr_output = &output->wlr_output;

  output->epd_device = *epd_init(epd_path, epd_vcom);
  unsigned int width = epd_output_get_width(wlr_output);
  unsigned int height = epd_output_get_height(wlr_output);

  output->egl_surface = egl_create_surface(&backend->egl, width, height);
  if (output->egl_surface == EGL_NO_SURFACE) {
    wlr_log(WLR_ERROR, "Failed to create EGL surface");
    goto error;
  }

  output_set_custom_mode(wlr_output, width, height, 0);
  strncpy(wlr_output->make, "epd", sizeof(wlr_output->make));
  strncpy(wlr_output->model, "epd", sizeof(wlr_output->model));
  snprintf(wlr_output->name, sizeof(wlr_output->name), "EPD-%zd",
           ++backend->last_output_num);

  if (!wlr_egl_make_current(&output->backend->egl, output->egl_surface, NULL)) {
    goto error;
  }

  wlr_renderer_begin(backend->renderer, wlr_output->width,
                     wlr_output->height);
  wlr_renderer_clear(backend->renderer, (float[]) { 1.0, 1.0, 1.0, 1.0 });
  wlr_renderer_end(backend->renderer);

  struct wl_event_loop *ev = wl_display_get_event_loop(backend->display);
  output->frame_timer = wl_event_loop_add_timer(ev, signal_frame, output);

  wl_list_insert(&backend->outputs, &output->link);

  if (backend->started) {
    wl_event_source_timer_update(output->frame_timer, output->frame_delay);
    wlr_output_update_enabled(wlr_output, true);
    wlr_signal_emit_safe(&backend->backend.events.new_output, wlr_output);
  }

  return wlr_output;

error:
  wlr_output_destroy(&output->wlr_output);
  return NULL;
}
