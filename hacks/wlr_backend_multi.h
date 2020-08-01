/*
 * This is a copy of the file ./include/backend/multi.h from the
 * wlroots project (https://github.com/swaywm/wlroots). We need this
 * header structure to be able to set a session for multi
 * backends. Without this, multi-backends with a session can only be
 * create via wlr_multi_backend_autocreate. This means that I can't
 * create a multi-backend with libinput + epd_backend.
 *
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 *
 * See the LICENSE file at the root of this repository.
 */

#ifndef HACK_BACKEND_MULTI_H
#define HACK_BACKEND_MULTI_H

#include <wayland-util.h>
#include <wlr/backend/interface.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>

struct wlr_multi_backend
{
  struct wlr_backend backend;
  struct wlr_session *session;

  struct wl_list backends;

  struct wl_listener display_destroy;

  struct
  {
    struct wl_signal backend_add;
    struct wl_signal backend_remove;
  } events;
};

#endif
