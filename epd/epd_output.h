#ifndef EPD_OUTPUT_H
#define EPD_OUTPUT_H

#include <wlr/backend/interface.h>
#include <epd/epd_backend.h>

struct epd_output
{
  struct wlr_output wlr_output;

  struct epd_backend *backend;
  struct wl_list link;

  void *egl_surface;
  struct wl_event_source *frame_timer;
  int frame_delay;              // ms
};


#endif
