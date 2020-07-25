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

struct epd_output
{
  struct wlr_output wlr_output;

  struct epd_backend *backend;
  struct wl_list link;

  void *egl_surface;
  struct wl_event_source *frame_timer;
  int frame_delay;              // ms
};

struct epd_input_device
{
  struct wlr_input_device wlr_input_device;

  struct epd_backend *backend;
};


#endif
