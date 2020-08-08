#ifndef EPD_UTILS_TIME_H
#define EPD_UTILS_TIME_H


#include <stdlib.h>
#include <time.h>
#include <unistd.h>


void timespec_diff(
  struct timespec *start,
  struct timespec *stop,
  struct timespec *result
);


#endif
