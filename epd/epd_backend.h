#ifndef EPD_BACKEND_H
#define EPD_BACKEND_H

#include <wlr/backend/interface.h>

#define EPD_BACKEND_DEFAULT_REFRESH (1 * 1000)  // 1 Hz

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

struct epd_backend *epd_backend_from_backend(
  struct wlr_backend *wlr_backend
);

struct wlr_backend *epd_backend_create(
  struct wl_display *display,
  wlr_renderer_create_func_t create_renderer_func
);

#endif
