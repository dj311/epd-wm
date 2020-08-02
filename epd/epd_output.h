#ifndef EPD_OUTPUT_H
#define EPD_OUTPUT_H

#include <pixman.h>
#include <wlr/backend/interface.h>

#include <epd/epd_backend.h>
#include <epd/epd_driver.h>

struct epd_output
{
  struct wlr_output wlr_output;

  struct epd_backend *backend;
  struct wl_list link;

  struct wl_event_source *frame_timer;
  int frame_delay;              // ms

  // Represents actual, physical display device.
  epd epd;

  // This is the surface/pixel buffer used inside the GPU for
  // compositing etc. In color.
  void *egl_surface;

  // This is an intermediate buffer used to store to store a copy of
  // the GPU's pixels in memory. In color.
  pixman_image_t *shadow_surface;

  // This stores a copy of the grayscale, pre-processed pixels taken
  // from shadow_surface. These are the raw bytes sent to the epd.
  // In grayscale, one byte per pixel.
  unsigned char *epd_pixels;
};

bool output_is_epd(
  struct wlr_output *wlr_output
);

struct epd_output *epd_output_from_output(
  struct wlr_output *wlr_output
);

unsigned int epd_output_get_width(
  struct wlr_output *wlr_output
);


unsigned int epd_output_get_height(
  struct wlr_output *wlr_output
);

struct wlr_output *epd_backend_add_output(
  struct wlr_backend *wlr_backend,
  char epd_path[],
  unsigned int epd_vcom
);


#endif
