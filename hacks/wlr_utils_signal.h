/*
 * This is a copy of the file ./include/util/signal.h from the wlroots
 * project (https://github.com/swaywm/wlroots).
 *
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 *
 * See the LICENSE file at the root of this repository.
 */

#ifndef HACK_UTILS_SIGNAL_H
#define HACK_UTILS_SIGNAL_H

void wlr_signal_emit_safe(
  struct wl_signal *signal,
  void *data
);

#endif
