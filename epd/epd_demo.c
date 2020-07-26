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
  printf("pgm_load: ./utils/image.pgm\n");
  pgm *image = pgm_load("./utils/image.pgm");

  printf("epd_init:\n");
  epd *display = epd_init("/dev/sg1");
  if (display == NULL) {
    printf("epd_init: failed\n");
    return -1;
  }

  printf("epd_draw:\n");
  for (unsigned int x = 10; x < ntohl(display->info.width); x += image->width) {
    for (unsigned int y = 10; y < ntohl(display->info.height);
         y += image->height) {
      int draw_status =
        epd_draw(display, x, y, image, EPD_UPD_EIGHT_BIT_FAST);
      if (draw_status != 0) {
        printf("epd_draw: failed\n");
      }
    }
  }

  printf("epd_reset:\n");
  int reset_status = epd_reset(display);
  if (reset_status != 0) {
    printf("epd_reset: failed\n");
  }

  if (close(display->fd) != 0) {
    printf("failed to close display fd whilst exiting\n");
  }

  free(display);

  return 0;
}
