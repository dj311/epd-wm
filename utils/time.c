/*
 * epd-wm: a Wayland window manager for IT8951 E-Paper displays
 *
 * Copyright (C) 2020 Daniel Jones
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void
timespec_diff(
  struct timespec *start,
  struct timespec *stop,
  struct timespec *result
)
{
  // Sourced from: https://gist.github.com/diabloneo/9619917
  // License unknown.

  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }

  return;
}
