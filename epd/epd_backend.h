#ifndef EPD_BACKEND_H
#define EPD_BACKEND_H

#include <wlr/backend/interface.h>

#define EPD_BACKEND_DEFAULT_REFRESH (60 * 1000) // 60 Hz

struct epd_backend
{
  struct wlr_backend backend;
  struct wlr_egl egl;
  struct wlr_renderer *renderer;
  struct wl_display *display;
  struct wl_list outputs;
  size_t last_output_num;
  struct wl_list input_devices;
  struct wl_listener display_destroy;
  bool started;
};


#endif
