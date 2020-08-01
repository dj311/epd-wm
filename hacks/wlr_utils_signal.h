#ifndef HACK_UTILS_SIGNAL_H
#define HACK_UTILS_SIGNAL_H

void wlr_signal_emit_safe(
  struct wl_signal *signal,
  void *data
);

#endif
