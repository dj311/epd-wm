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
  epd *display = epd_init("/dev/sg1");
  if (display == NULL) {
    printf("epd_init: failed\n");
    return -1;
  }

  printf("epd_reset:\n");
  int status = epd_reset(display);
  if (status != 0) {
    printf("epd_reset: failed\n");
  }

  pgm *image;

  int one_bit_values[2] = { 30 * 8, 0 * 8 };

  for (int u = 0; u < 2; u++) {
    for (int p = 0; p < 2; p++) {
      image =
        pgm_generate_solid_color(one_bit_values[p % 2],
                                 ntohl(display->info.width),
                                 ntohl(display->info.height));

      printf("epd_draw: u=%i, p=%i \n", EPD_ONE_BIT_MODES[u],
             one_bit_values[p % 2]);
      status = epd_draw(display, 0, 0, image, EPD_ONE_BIT_MODES[u]);

      sleep(1);
      free(image);
    }
  }

  int two_bit_values[4] = { 10 * 8, 20 * 8, 30 * 8, 0 * 8 };

  for (int u = 0; u < 1; u++) {
    for (int p = 0; p < 4; p++) {
      image =
        pgm_generate_solid_color(two_bit_values[p % 4],
                                 ntohl(display->info.width),
                                 ntohl(display->info.height));

      printf("epd_draw: u=%i, p=%i \n", EPD_TWO_BIT_MODES[u],
             two_bit_values[p % 4]);
      status = epd_draw(display, 0, 0, image, EPD_TWO_BIT_MODES[u]);

      sleep(1);
      free(image);
    }
  }

  int four_bit_values[16] = {
    2 * 8, 4 * 8, 6 * 8, 8 * 8, 10 * 8, 12 * 8, 14 * 8, 16 * 8, 18 * 8,
    20 * 8, 22 * 8, 24 * 8, 26 * 8, 28 * 8, 30 * 8, 0 * 8
  };

  for (int u = 0; u < 4; u++) {
    for (int p = 0; p < 16; p++) {
      image =
        pgm_generate_solid_color(four_bit_values[p % 16],
                                 ntohl(display->info.width),
                                 ntohl(display->info.height));

      printf("epd_draw: u=%i, p=%i \n", EPD_FOUR_BIT_MODES[u],
             four_bit_values[p % 16]);
      status = epd_draw(display, 0, 0, image, EPD_FOUR_BIT_MODES[u]);

      sleep(1);
      free(image);
    }
  }


  free(display);

  return 0;
}
