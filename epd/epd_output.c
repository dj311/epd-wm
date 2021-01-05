/*
 * Copyright (c) 2020 Daniel Jones
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include <epd/epd_driver.h>
#include <epd/epd_backend.h>
#include <epd/epd_output.h>

#include <utils/time.h>
#include <utils/pgm.h>
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


/* From file:~/programming/wlroots/include/wlr/types/wlr_output.h::struct wlr_output

   > A compositor output region. This typically corresponds to a monitor that
   > displays part of the compositor space.
   >
   > The `frame` event will be emitted when it is a good time for the compositor
   > to submit a new frame.
   >
   > To render a new frame, compositors should call `wlr_output_attach_render`,
   > render and call `wlr_output_commit`. No rendering should happen outside a
   > `frame` event handler or before `wlr_output_attach_render`.

   Which tells us more about the roles of indiviudal output functions. So
   I think the headless implementation of attach_render is fine. Need to
   make commit send the data to the epd, then we're good right? right?

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
output_set_custom_mode(
  struct wlr_output *wlr_output,
  int32_t width,
  int32_t height,
  int32_t refresh
)
{
  wlr_log(WLR_INFO, "Setting mode for epd output");
  struct epd_output *output = epd_output_from_output(wlr_output);
  struct epd_backend *backend = output->backend;

  if (refresh <= 0) {
    refresh = EPD_BACKEND_DEFAULT_REFRESH;
  }
  output->frame_delay = 1000000 / refresh;

  /* We use three pixel buffers (euugh). They each need to be set up
     now. This handles the initial setup (as output_set_custom_mode is
     called on output creation by epd_backend_add_output) and when the
     mode is changed.
   */

  /* GPU buffer for compositing */
  if (output->egl_surface) {
    wlr_egl_destroy_surface(&backend->egl, output->egl_surface);
  }

  output->egl_surface = egl_create_surface(&backend->egl, width, height);

  if (output->egl_surface == EGL_NO_SURFACE) {
    wlr_log(WLR_ERROR, "Failed to recreate EGL surface");
    wlr_output_destroy(wlr_output);
    return false;
  }

  /* CPU copy to work on without requiring a lock on the GPU buffer */
  if (output->shadow_surface) {
    pixman_image_unref(output->shadow_surface);
  }

  output->shadow_surface = pixman_image_create_bits(PIXMAN_x8r8g8b8,
                                                    width, height, NULL,
                                                    width * 4);

  /* Grayscale copy storing the exact bytes we send to the display */
  if (output->epd_pixels) {
    free(output->epd_pixels);
  }

  int pixels_size = sizeof(unsigned char) * width * height * 1;
  output->epd_pixels = malloc(pixels_size);
  memset(output->epd_pixels, 255, pixels_size);

  wlr_log(WLR_INFO, "Setting mode for epd output: success");

  wlr_output_update_custom_mode(&output->wlr_output, width, height, refresh);
  return true;
}

static bool
output_attach_render(
  struct wlr_output *wlr_output,
  int *buffer_age
)
{
  wlr_log(WLR_INFO, "epd_output: output_attach_render");
  struct epd_output *output = epd_output_from_output(wlr_output);
  bool ret = wlr_egl_make_current(&output->backend->egl, output->egl_surface,
                                  buffer_age);
  wlr_log(WLR_INFO, "epd_output: output_attach_render: complete");
  return ret;
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

     I should look at the drm implementation to what they do
     here. Ended up looking and heavily borrowing from the rdp
     implementation.
   */

  struct timespec time_commit_start;
  clock_gettime(CLOCK_REALTIME, &time_commit_start);

  wlr_log(WLR_INFO, "epd_commit: output_commit");
  struct epd_output *output = epd_output_from_output(wlr_output);

  unsigned int width = epd_output_get_width(wlr_output);
  unsigned int height = epd_output_get_height(wlr_output);

  wlr_log(WLR_INFO, "epd_commit: w=%i, h=%i", width, height);

  pixman_region32_t output_region;
  pixman_region32_init(&output_region);
  pixman_region32_union_rect(&output_region, &output_region, 0, 0, width,
                             height);

  /* Whats the damage? */
  pixman_region32_t *damage = &output_region;
  if (wlr_output->pending.committed & WLR_OUTPUT_STATE_DAMAGE) {
    damage = &wlr_output->pending.damage;
  }

  unsigned int dx = damage->extents.x1;
  unsigned int dy = damage->extents.y1;
  unsigned int dwidth = damage->extents.x2 - damage->extents.x1;
  unsigned int dheight = damage->extents.y2 - damage->extents.y1;

  if (dwidth == 0 || dheight == 0) {
    wlr_log(WLR_INFO, "epd_commit: no damage so finishing early");
    goto complete;
  }

  wlr_log(WLR_INFO,
          "epd_commit: reported damage dx=%u, dy=%u, dwidth=%u, dheight=%u",
          dx, dy, dwidth, dheight);

  /* Pull the damaged area into our CPU local, shadow surface */
  struct wlr_renderer *renderer =
    wlr_backend_get_renderer(&output->backend->backend);

  wlr_log(WLR_INFO, "epd_commit: copying gpu pixels to shadow buffer");

  uint32_t *shadow_pixels = pixman_image_get_data(output->shadow_surface);
  uint32_t shadow_stride = pixman_image_get_stride(output->shadow_surface);

  // Always pull in the whole buffer, to avoid the bug in
  // https://github.com/swaywm/wlroots/pull/1809
  struct timespec time_read_pixels_start;
  clock_gettime(CLOCK_REALTIME, &time_read_pixels_start);

  int read_pixels_success =
    wlr_renderer_read_pixels(renderer, WL_SHM_FORMAT_XRGB8888, NULL,
                             shadow_stride,
                             width, height, 0, 0, 0, 0,
                             shadow_pixels);

  struct timespec time_read_pixels_end;
  clock_gettime(CLOCK_REALTIME, &time_read_pixels_end);

  if (!read_pixels_success) {
    wlr_log(WLR_INFO,
            "epd_commit: wlr_renderer_read_pixels returned falsy. cannot read pixels to update so skipping this commit");
    goto complete;
  }

  /* Now transfer the damaged ARGB pixels into the greyscale buffer */
  struct timespec time_damage_start;
  clock_gettime(CLOCK_REALTIME, &time_damage_start);

  wlr_log(WLR_INFO, "epd_commit: copying shadow pixels to epd buffer");
  unsigned int location;

  /* These help us with manual damage tracking */
  unsigned int dxmin = dx + dwidth;
  unsigned int dxmax = dx;
  unsigned int dymin = dy + dheight;
  unsigned int dymax = dy;

  int update_mode = EPD_UPD_DU4;
  unsigned char r, g, b;
  unsigned char new_value;

  for (unsigned int x = dx; x < dx + dwidth; x++) {
    for (unsigned int y = dy; y < dy + dheight; y++) {
      location = x + width * y;

      /* Each pixel in pixman/egl buffers is 32 bits consisting of 4
         bytes, each representing the a, r, g, b components. Extract r,
         g, b and average to get our grayscale value.
       */
      r = *(&shadow_pixels[location] + 1);
      g = *(&shadow_pixels[location] + 2);
      b = *(&shadow_pixels[location] + 3);

      new_value = (r + g + b) / 3;

      // EPD_ONE_BIT_MODES
      if (update_mode == EPD_UPD_DU || update_mode == EPD_UPD_A2) {
        new_value = pgm_filter_one_bit_pixel(new_value);
      }
      // EPD_TWO_BIT_MODES
      if (update_mode == EPD_UPD_DU4) {
        new_value = pgm_filter_two_bit_pixel(new_value);
      }
      // EPD_FOUR_BIT_MODES
      if (update_mode == EPD_UPD_GC16 || update_mode == EPD_UPD_GL16
          || update_mode == EPD_UPD_GLR16 || update_mode == EPD_UPD_GLD16) {
        new_value = pgm_filter_four_bit_pixel(new_value);
      }

      /* Update damage tracking if this pixel is damaged */
      if (new_value != output->epd_pixels[location]) {
        if (x < dxmin)
          dxmin = x;

        if (x > dxmax)
          dxmax = x;

        if (y < dymin)
          dymin = y;

        if (y > dymax)
          dymax = y;
      }

      output->epd_pixels[location] = new_value;
    }
  }
  struct timespec time_damage_end;
  clock_gettime(CLOCK_REALTIME, &time_damage_end);

  /* If these values haven't changed, there is no damage: */

  if (dxmin == dx + dwidth || dxmax == dx || dymin == dy + dheight
      || dymax == dy) {
    wlr_log(WLR_INFO,
            "epd_commit: calculated damage suggests no changes, no damage so finishing early");
    goto complete;
  }

  /* Update damage info with our new information */
  dx = dxmin;
  dy = dymin;
  dwidth = dxmax - dxmin + 1;
  dheight = dymax - dymin + 1;

  wlr_log(WLR_INFO,
          "epd_commit: calculated damage dx=%u, dy=%u, dwidth=%u, dheight=%u",
          dx, dy, dwidth, dheight);

  /* Send pixels, then display on the epd */
  wlr_log(WLR_INFO, "epd_commit: sending update to display");
  struct timespec time_send_pixels_start;
  clock_gettime(CLOCK_REALTIME, &time_send_pixels_start);
  epd_fast_copy_image_bytes(&output->epd, output->epd_pixels, dx + dy * width,
                            (dx + dwidth) + (dy + dheight) * width);
  struct timespec time_display_start;
  clock_gettime(CLOCK_REALTIME, &time_display_start);
  epd_display_area(&output->epd, dx, dy, dwidth, dheight, update_mode, 1);
  struct timespec time_display_end;
  clock_gettime(CLOCK_REALTIME, &time_display_end);
  wlr_log(WLR_INFO, "epd_commit: display update sent");

  wlr_log(WLR_INFO, "epd_commit: timing report");

  /* Timing report */
  struct timespec time_commit;
  timespec_diff(&time_commit_start, &time_display_end, &time_commit);
  wlr_log(WLR_INFO, "epd_commit: time_commit = %llis %llims",
          (long long) time_commit.tv_sec, time_commit.tv_nsec / 1000000);

  struct timespec time_read_pixels;
  timespec_diff(&time_read_pixels_start, &time_read_pixels_end,
                &time_read_pixels);
  wlr_log(WLR_INFO, "epd_commit: time_read_pixels = %llis %llims",
          (long long) time_read_pixels.tv_sec,
          time_read_pixels.tv_nsec / 1000000);

  struct timespec time_damage;
  timespec_diff(&time_damage_start, &time_damage_end, &time_damage);
  wlr_log(WLR_INFO, "epd_commit: time_damage = %llis %llims",
          (long long) time_damage.tv_sec, time_damage.tv_nsec / 1000000);

  struct timespec time_send_pixels;
  timespec_diff(&time_send_pixels_start, &time_display_start,
                &time_send_pixels);
  wlr_log(WLR_INFO, "epd_commit: time_send_pixels = %llis %llims",
          (long long) time_send_pixels.tv_sec,
          time_send_pixels.tv_nsec / 1000000);

  struct timespec time_display;
  timespec_diff(&time_display_start, &time_display_end, &time_display);
  wlr_log(WLR_INFO, "epd_commit: time_display = %llis %llims",
          (long long) time_display.tv_sec, time_display.tv_nsec / 1000000);

  goto complete;

complete:
  wlr_log(WLR_INFO, "epd_commit: commit complete - success");
  wlr_output_send_present(wlr_output, NULL);
  return true;
}

static void
output_destroy(
  struct wlr_output *wlr_output
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);

  free(output->epd_pixels);
  epd_reset(&output->epd);
  epd_pmic_off(&output->epd);
  close(output->epd.fd);

  wl_list_remove(&output->link);

  wl_event_source_remove(output->frame_timer);

  pixman_image_unref(&output->shadow_surface);

  wlr_egl_destroy_surface(&output->backend->egl, output->egl_surface);

  free(output);
}

static int
signal_frame(
  void *data
)
{
  /*
     This function schedules: the output should update its frames
     every output->frame_delay <units> (I don't know what units it is
     in).
   */
  wlr_log(WLR_INFO, "epd_output: signal_frame");
  struct epd_output *output = data;
  wlr_output_send_frame(&output->wlr_output);
  wl_event_source_timer_update(output->frame_timer, output->frame_delay);
  return 0;
}

static const struct wlr_output_impl output_impl = {
  .set_custom_mode = output_set_custom_mode,
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
  return ntohl(output->epd.info.width);
}


unsigned int
epd_output_get_height(
  struct wlr_output *wlr_output
)
{
  struct epd_output *output = epd_output_from_output(wlr_output);
  return ntohl(output->epd.info.height);
}

struct wlr_output *
epd_backend_add_output(
  struct wlr_backend *wlr_backend,
  char epd_path[],
  unsigned int epd_vcom
)
{
  wlr_log(WLR_INFO, "Adding output to epd backend");
  struct epd_backend *backend = epd_backend_from_backend(wlr_backend);

  /* Initialise the epd_output struct in memory */
  struct epd_output *output = calloc(1, sizeof(struct epd_output));
  if (output == NULL) {
    wlr_log(WLR_ERROR, "Failed to allocate epd_output");
    return NULL;
  }

  /* Add backlink to the backend */
  output->backend = backend;

  wlr_output_init(&output->wlr_output, &backend->backend, &output_impl,
                  backend->display);
  struct wlr_output *wlr_output = &output->wlr_output;

  /* Initialise the epd and steal its config info */
  wlr_log(WLR_INFO, "Initialise the epd display");
  epd_init(&output->epd, epd_path, epd_vcom);
  wlr_log(WLR_INFO, "Initialise the epd display: success");

  unsigned int width = epd_output_get_width(wlr_output);
  unsigned int height = epd_output_get_height(wlr_output);

  wlr_log(WLR_INFO, "Clear the epd display");
  epd_reset(&output->epd);

  /* This sets up all our buffers as needed by the video mode */
  wlr_log(WLR_INFO, "Set custom mode");
  output_set_custom_mode(wlr_output, width, height, 0);

  /* Metadata */
  strncpy(wlr_output->make, "epd-todo", sizeof(wlr_output->make));
  strncpy(wlr_output->model, "epd-todo", sizeof(wlr_output->model));
  snprintf(wlr_output->name, sizeof(wlr_output->name), "EPD-%zd",
           ++backend->last_output_num);

  /* Set up the renderer */
  wlr_log(WLR_INFO, "Set output egl surface to current for the backend");
  if (!wlr_egl_make_current(&output->backend->egl, output->egl_surface, NULL)) {
    goto error;
  }

  wlr_log(WLR_INFO, "Clear the output egl surface");
  wlr_renderer_begin(backend->renderer, wlr_output->width,
                     wlr_output->height);
  wlr_renderer_clear(backend->renderer, (float[]) { 1.0, 1.0, 1.0, 1.0 });
  wlr_renderer_end(backend->renderer);

  /* Here we add an item to the wayland event loop: our signal_frame
     function will be run every output->frame_delay (which is actually
     EPD_BACKEND_DEFAULT_REFRESH).
   */

  wlr_log(WLR_INFO,
          "Hook up timer to send frames at EPD_BACKEND_DEFAULT_REFRESH");
  struct wl_event_loop *ev = wl_display_get_event_loop(backend->display);
  output->frame_timer = wl_event_loop_add_timer(ev, signal_frame, output);

  wl_list_insert(&backend->outputs, &output->link);

  /* Start up */
  if (backend->started) {
    wl_event_source_timer_update(output->frame_timer, output->frame_delay);
    wlr_output_update_enabled(wlr_output, true);
    wlr_signal_emit_safe(&backend->backend.events.new_output, wlr_output);
  }

  wlr_log(WLR_INFO, "Adding output to epd backend: success");
  return wlr_output;

error:
  wlr_output_destroy(&output->wlr_output);
  return NULL;
}
