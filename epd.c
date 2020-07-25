#include<ctype.h>
#include<fcntl.h>
#include<scsi/scsi_ioctl.h>
#include<scsi/sg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<./pgm.h>
#include<./epd.h>

/* SCSI Generic ----------------------------------------------------------------

A bunch of tools and utilities for playing with SCSI generic
devices. Most of the stuff we need is in stuff is in scsi/sg.h so this
should be quite minimal.

#+begin_quote
    SCSI generic driver supports many typical system calls for
    character device, such as open(), close(), read(), write, poll(),
    ioctl(). The procedure for sending SCSI commands to a specific
    SCSI device is also very simple:

    1. Open the SCSI generic device file (such as sg1) to get the file
        descriptor of SCSI device.
    2. Prepare the SCSI command.
    3. Set related memory buffers.
    4. Call the ioctl() function to execute the SCSI command.
    5. Close the device file.
#+end_quote

A good, if not verbose, reference for this is the actual Linux source
code. Here is the file & commit this library was written against:
    https://github.com/torvalds/linux/blob/6f0d349d922ba44e4348a17a78ea51b7135965b1/include/scsi/sg.h#L44

*/

// SCSI operation codes (used as the first byte of an CommandDescriptorBlock).
enum sg_opcode
{
  SG_OP_INQUIRY = 0x12,
  SG_OP_CUSTOM = 0xFE
};


int
send_message(
  int fd,
  int command_length,
  unsigned char *command_pointer,
  int data_direction,
  int data_length,
  unsigned char *data_pointer
)
{
  // In cases of error, the sense buffer may be filled by the kernel or device.
  int sense_length = 100;
  unsigned char sense[100] = { 0 };

  // Construct Message Header
  struct sg_io_hdr *message_pointer =
    (struct sg_io_hdr *) malloc(sizeof(struct sg_io_hdr));

  memset(message_pointer, 0, sizeof(struct sg_io_hdr));

  message_pointer->interface_id = 'S';
  message_pointer->flags = SG_FLAG_DIRECT_IO;
  message_pointer->timeout = 5;
  message_pointer->pack_id = 0;
  message_pointer->usr_ptr = NULL;
  message_pointer->iovec_count = 0;

  message_pointer->cmd_len = command_length;
  message_pointer->cmdp = command_pointer;

  message_pointer->dxfer_direction = data_direction;
  message_pointer->dxfer_len = data_length;
  message_pointer->dxferp = data_pointer;

  message_pointer->mx_sb_len = sense_length;
  message_pointer->sbp = &sense;

  int status = ioctl(fd, SG_IO, message_pointer);

  return status;
}


/* IT8951 EPD Driver -----------------------------------------------------------

*/


struct epd_display_area_args_addr
{
  unsigned_char[4] address;
  unsigned_char[4] update_mode; // get from GetSysResponse.uiMode
  unsigned_char[4] x;
  unsigned_char[4] y;
  unsigned_char[4] width;
  unsigned_char[4] height;
  unsigned_char[4] wait_display_ready;  // set to 1 to enable
};


struct epd_load_image_args_addr
{
  unsigned char[4] address;
  unsigned char[4] x;
  unsigned char[4] y;
  unsigned char[4] width;
  unsigned char[4] height;
  unsigned char *pixels;
};


enum epd_update_mode
{
  // Clear Mode:
  EPD_UPD_RESET = 0,

  // 8 bits per pixel:
  EPD_UPD_EIGHT_BIT_SLOW = 2,
  EPD_UPD_EIGHT_BIT_MEDIUM = 3,

  // Does full update of area requested but only
  // updates the non-white values.
  //  - Maybe not actually sure. Seems more
  //    complicated.
  EPD_UPD_EIGHT_BIT_FAST = 4,
  EPD_UPD_TWO_BIT = 5,

  // Below values require < 8 bits per pixel.
  EPD_UPD_ONE_BIT = 1,          // Binary

  // I think the following is good for 1-bit data
  // but only works with 1-bit data (e.g. if you
  // give it grey values it will do nothing with
  // them).
  //   - No, not quite.
  EPD_UPD_ONE_BIT_FAST = 6,     // Fast Binary?

  // Seems to do thresholding of some sort (maybe
  // 2 or 3 bits?)
  // - Does 7 apply a binary mask to your next update? No
  // - Supports two bits per pixel I believe
  // - Does it do an auto diff with the image buffer?
  EPD_UPD_TWO_OR_FOUR_BITS = 7, // 2-bits per pixel
};

// Fill this code to CDB[6].
enum epd_opcode
{
  EPD_OP_GET_SYS = 0x80,
  EPD_OP_READ_MEM = 0x81,
  EPD_OP_WRITE_MEM = 0x82,
  EPD_OP_DPY_AREA = 0x94,
  EPD_OP_LD_IMG_AREA = 0xA2,
  EPD_OP_PMIC_CTRL = 0xA3,
  EPD_OP_FAST_WRITE_MEM = 0xA5,
  EPD_OP_AUTO_RESET = 0xA7,

  // Following are not listed in the docs, but found in the example
  // code. Danger?
  EPD_OP_USB_SPI_ERASE = 0x96,
  EPD_OP_USB_SPI_READ = 0x97,
  EPD_OP_USB_SPI_WRITE = 0x98,
  EPD_OP_IMG_CPY = 0xA4,        // Not used in Current Version (IT8961 Samp only)
  EPD_OP_READ_REG = 0x83,
  EPD_OP_WRITE_REG = 0x84,
  EPD_OP_FSET_TEMP = 0xA4,
};

// TODO: epd_info struct is wrong length
struct epd_info
{
  unsigned int standard_cmd_number;
  unsigned int extend_cmd_number;
  unsigned int signature;
  unsigned int version;
  unsigned int width;
  unsigned int height;
  unsigned int update_buffer_address;
  unsigned int image_buffer_address;
  unsigned int temperature_segment_number;
  unsigned int display_modes_count;
  unsigned int display_mode_frame_counts[8];
  unsigned int image_buffers_count;
  unsigned int wbf_sfi_address;
  unsigned int reserved[9];
  void *cmd_info_data[1];
};


enum epd_state
{ EPD_INIT, EPD_READY, EPD_BUSY };


struct epd
{
  int fd;
  int state;
  unsigned int max_transfer;
  struct epd_info info;
};

int
epd_transfer_image_chunk(
  struct epd *display,
  unsigned int base_x,
  unsigned int base_y,
  unsigned int chunk_x,
  unsigned int chunk_y,
  unsigned int chunk_width,
  unsigned int chunk_height,
  struct pgm image,
)
{
}


int
epd_transfer_image(
  struct epd *display,
  unsigned int x,
  unsigned int y,
  struct pgm image,
)
{

  // Transfer the image given in `image` into the image buffer in
  // `epd`s memory, at the offset determined by `x` and `y`.

  // Chunked based on the max transfer size of the display, taking
  // whole rows of the input image (under the assumption that
  // image->width < epd->max_tranfer).

  unsigned int max_chunk_height = epd->max_transfer / image->width;

  unsigned int start_row, end_row;
  unsigned int start_x, start_y;
  unsigned int chunk_width, chunk_height;
  unsigned int chunk_address;

  for (start_row = 0; start_row < image->height;
       start_row += max_chunk_height) {

    end_row = start_row + max_chunk_height;
    if (end_row > image->height) {
      end_row = image->height;
    }

    chunk_width = image->width;
    chunk_height = end_row - start_row;

    chunk_address_in_src_image = 0 + start_row * chunk_width;

    chunk_address_in_epd_image = x + (y + start_row) * epd->info->width;
    chunk_address_in_epd_memory =
      epd->info->image_buffer_address + chunk_address_in_epd_image;

    unsigned char load_image_command[16] = {
      SG_OP_CUSTOM, 0, 0, 0, 0, 0,
      EPD_OP_LD_IMG_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    int num_pixels = chunk_width * chunk_height;
    int args_length = sizeof(struct epd_load_image_args_addr) + num_pixels;

    struct epd_load_image_args_addr *load_image_args = malloc(args_length);
    load_image_args->address = chunk_address_in_epd_memory;
    load_image_args->x = x;
    load_image_args->y = y + start_row;
    load_image_args->width = chunk_width;
    load_image_args->height = chunk_height;
    load_image_args->pixels = image->pixels + chunk_address_in_src_image;

    int status = send_message(display->fd,
                              16,
                              &load_image_command,
                              SG_DXFER_TO_DEV,
                              args_length,
                              &load_image_args);

    if (status != 0) {
      printf("epd_transfer_image: failed to send chunk %u\n", start_row);
      return -1;
    }

  }

  return 0;
}


int
epd_draw(
  struct epd *display,
  unsigned int x,
  unsigned int y,
  struct pgm image,
  struct epd_update_mode update_mode,
)
{
  if (display->state != EPD_READY) {
    return -1;
  }

  if (image->bytes_per_pixel != 1) {
    printf("epd_draw: only supports images with 1 byte per pixel\n");
    return -1;
  }

  if (image->x + image->width > epd->info->width
      || image->y + image->height > epd->info->height) {
    printf("epd_draw: cannot draw image outside of display boundary\n");
    return -1;
  }

  if (image->x == 0 && image->y == 0 && image->width == epd->info->width
      && image->height == epd->info->height) {
    printf("epd_draw: detected full image update\n");
    // TODO: Can we optimise this case? I believe there are special ops we can do.
  }

  unsigned char draw_command[16] = {
    SG_OP_CUSTOM, 0, 0, 0, 0, 0, EPD_OP_DPY_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  int status = send_message(display->fd, 16, &draw_command, SG_DXFER_TO_DEV,
                            info_length, &(display->info));

  if (status != 0) {
    return -1;
  }

  return 0;

}


int
epd_get_system_info(
  struct epd *display
)
{
  if (display->state != EPD_INIT) {
    return -1;
  }

  int info_length = sizeof(struct epd_info);

  unsigned char info_command[16] = {
    SG_OP_CUSTOM, 0, 0x38, 0x39, 0x35, 0x31, EPD_OP_GET_SYS, 0, 0x01, 0, 0x02,
    0, 0, 0, 0, 0
  };

  int status = send_message(display->fd, 16, &info_command, SG_DXFER_FROM_DEV,
                            info_length, &(display->info));

  if (status != 0) {
    return -1;
  }

  return 0;
}


int
epd_ensure_it8951_display(
  struct epd *display
)
{
  if (display->state != EPD_INIT) {
    return -1;
  }

  unsigned char inquiry_response[40] = { 0 };
  unsigned char inquiry_command[16] =
    { 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  int status =
    send_message(display->fd, 16, &inquiry_command, SG_DXFER_FROM_DEV, 40,
                 &inquiry_response);

  if (status != 0) {
    return -1;
  }

  char *device_name = malloc(28);
  strncpy(device_name, inquiry_response + 8, 28);

  if (strcmp(device_name, "Generic Storage RamDisc 1.00") != 0) {
    return -1;
  }

  return 0;
}


struct epd *
epd_init(
  char path[]
)
{
  struct epd *display = (struct epd *) malloc(sizeof(struct epd));
  memset(display, 0, sizeof(struct epd));

  display->fd = open(path, O_RDWR);
  display->state = EPD_INIT;
  display->max_transfer = 60000;

  if (epd_ensure_it8951_display(display) != 0) {
    free(display);
    return NULL;
  }

  epd_get_system_info(display);

  printf("width=%u, height=%u\n", ntohl(display->info.width),
         ntohl(display->info.height));

  display->state = EPD_READY;

  return display;
}


// wlroots interface -------------------------------------------------------- //



// demo --------------------------------------------------------------------- //

int
main(
)
{
  printf("Hello, Dan!\n");

  struct pgm *image = pgm_load("./image.pgm");
  pgm_print(image);

  /* struct epd *display = epd_init("/dev/sg1"); */
  /* if (display == NULL) { */
  /*   return -1; */
  /* } */

  return 0;
}
