#ifndef EPD_OUTPUT_H
#define EPD_OUTPUT_H

#include <wlr/backend/interface.h>
#include <epd/epd_backend.h>
#include <epd/epd_driver.h>

struct epd_output
{
  struct wlr_output wlr_output;

  struct epd_backend *backend;
  struct wl_list link;

  void *egl_surface;
  struct wl_event_source *frame_timer;
  int frame_delay;              // ms

  epd epd_device;
};

bool output_is_epd(
  struct wlr_output *wlr_output
);

static struct epd_output *epd_output_from_output(
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
