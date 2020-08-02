/*
 * epd-wm: a Wayland window manager for IT8951 E-Paper displays
 *
 * Copyright (C) 2020 Daniel Jones
 *
 * See the LICENSE file accompanying this file.
 */

#include<arpa/inet.h>
#include<ctype.h>
#include<fcntl.h>
#include<scsi/scsi_ioctl.h>
#include<scsi/sg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<utils/pgm.h>
#include<epd/epd_driver.h>

int
main(
)
{
  printf("epd_init:\n");

  epd *display = (epd *) malloc(sizeof(epd));
  memset(display, 0, sizeof(epd));
  epd_init(display, "/dev/sg1", 1810);

  if (display == NULL) {
    printf("epd_init: failed\n");
    return -1;
  }

  printf("epd_reset:\n");
  int status = epd_reset(display);
  if (status != 0) {
    printf("epd_reset: failed\n");
  }
  sleep(1);

  int count = 0;
  pgm *image = pgm_load("./utils/image-one-bit.pgm");
  for (unsigned int x = 0; x + image->width <= ntohl(display->info.width);
       x += image->width) {
    for (unsigned int y = 0; y + image->height <= ntohl(display->info.height);
         y += image->height) {
      if (count < 10) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_A2);
      } else if (count < 20) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_DU);
      } else if (count < 30) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_DU4);
      }
      if (status != 0) {
        printf("epd_draw_pgm: failed\n");
      }
      count++;
    }
  }
  free(image->pixels);
  free(image);

  printf("count=%i\n", count);
  sleep(3);

  printf("epd_reset:\n");
  status = epd_reset(display);
  if (status != 0) {
    printf("epd_reset: failed\n");
  }
  sleep(1);

  count = 0;
  image = pgm_load("./utils/image-two-bit.pgm");
  for (unsigned int x = 0; x + image->width <= ntohl(display->info.width);
       x += image->width) {
    for (unsigned int y = 0; y + image->height <= ntohl(display->info.height);
         y += image->height) {
      if (count < 10) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_A2);
      } else if (count < 20) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_DU4);
      } else if (count < 30) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_GLR16);
      }
      if (status != 0) {
        printf("epd_dra_pgmw: failed\n");
      }
      count++;
    }
  }
  free(image->pixels);
  free(image);

  printf("count=%i\n", count);
  sleep(3);

  printf("epd_reset:\n");
  status = epd_reset(display);
  if (status != 0) {
    printf("epd_reset: failed\n");
  }
  sleep(1);

  count = 0;
  image = pgm_load("./utils/image-four-bit.pgm");
  for (unsigned int x = 0; x + image->width <= ntohl(display->info.width);
       x += image->width) {
    for (unsigned int y = 0; y + image->height <= ntohl(display->info.height);
         y += image->height) {
      if (count < 10) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_GL16);
      } else if (count < 20) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_GLR16);
      } else if (count < 30) {
        epd_draw_pgm(display, x, y, image, EPD_UPD_GLD16);
      }
      if (status != 0) {
        printf("epd_draw_pgm: failed\n");
      }
      count++;
    }
  }
  free(image->pixels);
  free(image);

  printf("count=%i\n", count);
  sleep(3);

  printf("epd_reset:\n");
  status = epd_reset(display);
  if (status != 0) {
    printf("epd_reset: failed\n");
  }

  free(display);

  return 0;
}
