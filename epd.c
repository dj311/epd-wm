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
typedef unsigned char sg_command;
typedef unsigned char sg_data;


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
  sg_command * command_pointer,
  int data_direction,
  int data_length,
  sg_data * data_pointer
)
{
  // In cases of error, the sense buffer may be filled by the kernel or device.
  int sense_length = 100;
  unsigned char sense[100] = { 0 };

  // Construct Message Header
  sg_io_hdr_t *message_pointer = (sg_io_hdr_t *) malloc(sizeof(sg_io_hdr_t));

  memset(message_pointer, 0, sizeof(sg_io_hdr_t));

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
  message_pointer->sbp = sense;

  int status = ioctl(fd, SG_IO, message_pointer);
  if (status != 0) {
    printf("send_message: failed with status %i\n", status);
  }

  free(message_pointer);

  return status;
}


/* IT8951 EPD Driver -----------------------------------------------------------

*/


typedef struct
{
  int address;
  int update_mode;              // get from GetSysResponse.uiMode
  int x;
  int y;
  int width;
  int height;
  int wait_display_ready;       // set to 1 to enable
} epd_display_area_args_addr;


typedef struct
{
  int address;
  int x;
  int y;
  int width;
  int height;
  unsigned char pixels[];
} epd_load_image_args_addr;


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
typedef struct
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
} epd_info;


enum epd_state
{ EPD_INIT, EPD_READY, EPD_BUSY };


typedef struct
{
  int fd;
  int state;
  unsigned int max_transfer;
  epd_info info;
} epd;


int
epd_transfer_image(
  epd * display,
  unsigned int x,
  unsigned int y,
  pgm * image
)
{

  // Transfer the image given in `image` into the image buffer in
  // `epd`s memory, at the offset determined by `x` and `y`.

  // Chunked based on the max transfer size of the display, taking
  // whole rows of the input image (under the assumption that
  // image->width < epd->max_tranfer).

  unsigned int image_address_le = ntohl(display->info.image_buffer_address);

  unsigned int max_chunk_height = display->max_transfer / image->width;

  unsigned int start_row, end_row;
  unsigned int chunk_width, chunk_height;

  printf("epd_transfer_image: %u %u %u %u %u %u\n",
         display->max_transfer, max_chunk_height, x, y, image->width,
         image->height);

  for (start_row = 0; start_row < image->height;
       start_row += max_chunk_height) {
    printf("epd_transfer_image: start_row=%u\n", start_row);

    end_row = start_row + max_chunk_height;
    if (end_row > image->height) {
      end_row = image->height;
    }

    chunk_width = image->width;
    chunk_height = end_row - start_row;

    unsigned long chunk_address_in_src_image = 0 + start_row * chunk_width;
    unsigned long chunk_address_in_our_memory =
      image->pixels + chunk_address_in_src_image;

    unsigned long chunk_address_in_epd_image =
      x + (y + start_row) * ntohl(display->info.width);
    unsigned long chunk_address_in_epd_memory =
      image_address_le + chunk_address_in_epd_image;

    sg_command load_image_command[16] = {
      SG_OP_CUSTOM, 0, 0, 0, 0, 0,
      EPD_OP_LD_IMG_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    int num_pixels = chunk_width * chunk_height;
    int args_length = sizeof(epd_load_image_args_addr) + num_pixels;

    epd_load_image_args_addr *load_image_args = malloc(args_length);
    load_image_args->address = htonl(chunk_address_in_epd_memory);
    load_image_args->x = htonl(x);
    load_image_args->y = htonl(y + start_row);
    load_image_args->width = htonl(chunk_width);
    load_image_args->height = htonl(chunk_height);
    memcpy(load_image_args->pixels,
           (unsigned char *) chunk_address_in_our_memory, num_pixels);

    int status = send_message(display->fd,
                              16,
                              load_image_command,
                              SG_DXFER_TO_DEV,
                              args_length,
                              (sg_data *) load_image_args);

    free(load_image_args);

    if (status != 0) {
      printf("epd_transfer_image: failed to send chunk %u so gave up\n",
             start_row);
      return -1;
    }

  }

  printf("epd_transfer_image: complete\n");
  return 0;
}


int
epd_draw(
  epd * display,
  unsigned int x,
  unsigned int y,
  pgm * image,
  enum epd_update_mode update_mode
)
{
  if (display->state != EPD_READY) {
    printf("epd_draw: display must be in EPD_READY state\n");
    return -1;
  }

  if (image->bytes_per_pixel != 1) {
    printf("epd_draw: only supports images with 1 byte per pixel\n");
    return -1;
  }

  if (x + image->width > display->info.width
      || y + image->height > display->info.height) {
    printf("epd_draw: cannot draw image outside of display boundary\n");
    return -1;
  }

  if (x == 0 && y == 0
      && image->width == display->info.width
      && image->height == display->info.height) {
    printf("epd_draw: detected full image update\n");
    // TODO: Can we optimise this case? I believe there are special ops we can do.
  }

  int transfer_success = epd_transfer_image(display, x, y, image);
  if (transfer_success != 0) {
    printf("epd_draw: failed to transfer image to device\n");
    return -1;
  }
  printf("epd_draw: transfer success\n");

  sg_command draw_command[16] = {
    SG_OP_CUSTOM, 0, 0, 0, 0, 0, EPD_OP_DPY_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  epd_display_area_args_addr draw_data;
  draw_data.address = display->info.image_buffer_address;
  draw_data.update_mode = htonl(update_mode);
  draw_data.x = htonl(x);
  draw_data.y = htonl(y);
  draw_data.width = htonl(image->width);
  draw_data.height = htonl(image->height);
  draw_data.wait_display_ready = 0;

  int status = send_message(display->fd,
                            16,
                            draw_command,
                            SG_DXFER_TO_DEV,
                            sizeof(epd_display_area_args_addr),
                            (sg_data *) & draw_data);

  if (status != 0) {
    return -1;
  }
  printf("epd_draw: draw success\n");

  return 0;

}


int
epd_reset(
  epd * display
)
{
  if (display->state != EPD_READY) {
    printf("epd_reset: display must be in EPD_READY state\n");
    return -1;
  }

  sg_command reset_command[16] = {
    SG_OP_CUSTOM, 0, 0, 0, 0, 0, EPD_OP_DPY_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  epd_display_area_args_addr reset_data;
  reset_data.address = display->info.image_buffer_address;
  reset_data.update_mode = EPD_UPD_RESET;
  reset_data.x = 0;
  reset_data.y = 0;
  reset_data.width = display->info.width;
  reset_data.height = display->info.height;
  reset_data.wait_display_ready = 1;

  int status = send_message(display->fd,
                            16,
                            reset_command,
                            SG_DXFER_TO_DEV,
                            sizeof(epd_display_area_args_addr),
                            (sg_data *) & reset_data);

  if (status != 0) {
    return -1;
  }

  printf("epd_reset: success\n");

  return 0;
}


int
epd_get_system_info(
  epd * display
)
{
  if (display->state != EPD_INIT) {
    return -1;
  }

  int info_length = sizeof(epd_info);

  unsigned char info_command[16] = {
    SG_OP_CUSTOM, 0, 0x38, 0x39, 0x35, 0x31, EPD_OP_GET_SYS, 0, 0x01, 0, 0x02,
    0, 0, 0, 0, 0
  };

  int status = send_message(display->fd,
                            16,
                            (sg_command *) info_command,
                            SG_DXFER_FROM_DEV,
                            info_length, (sg_data *) & (display->info));

  if (status != 0) {
    return -1;
  }

  return 0;
}


int
epd_ensure_it8951_display(
  epd * display
)
{
  if (display->state != EPD_INIT) {
    printf
      ("epd_ensure_it8951_display: display struct not in EPD_INIT state\n");
    return -1;
  }

  sg_command inquiry_command[16] =
    { 0x12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  sg_data inquiry_response[40] = { 0 };

  int status = send_message(display->fd,
                            16,
                            inquiry_command,
                            SG_DXFER_FROM_DEV,
                            40,
                            inquiry_response);

  if (status != 0) {
    printf("epd_ensure_it8951_display: inquiry msg failed\n");
    return -1;
  }

  char *device_name = malloc(28);
  strncpy(device_name, inquiry_response + 8, 28);

  if (strcmp(device_name, "Generic Storage RamDisc 1.00") != 0) {
    printf("epd_ensure_it8951_display: name doesn't match\n");
    free(device_name);
    return -1;
  }


  free(device_name);
  return 0;
}


epd *
epd_init(
  char path[]
)
{
  epd *display = (epd *) malloc(sizeof(epd));
  memset(display, 0, sizeof(epd));

  display->fd = open(path, O_RDWR);
  display->state = EPD_INIT;
  display->max_transfer = 60000;

  if (epd_ensure_it8951_display(display) != 0) {
    free(display);
    return NULL;
  }

  epd_get_system_info(display);

  printf("width=%u, height=%u\n",
         ntohl(display->info.width), ntohl(display->info.height));

  display->state = EPD_READY;

  return display;
}


// wlroots interface -------------------------------------------------------- //



// demo --------------------------------------------------------------------- //

int
main(
)
{
  printf("pgm_load: ./image.pgm\n");
  pgm *image = pgm_load("./image.pgm");

  printf("epd_init:\n");
  epd *display = epd_init("/dev/sg1");
  if (display == NULL) {
    printf("epd_init: failed\n");
    return -1;
  }

  printf("epd_reset:\n");
  int reset_status = epd_reset(display);
  if (reset_status != 0) {
    printf("epd_reset: failed\n");
  }

  printf("epd_draw:\n");
  for (unsigned int x = 0; x < ntohl(display->info.width); x += image->width) {
    for (unsigned int y = 0; y < ntohl(display->info.height);
         y += image->height) {
      int draw_status =
        epd_draw(display, x, y, image, EPD_UPD_EIGHT_BIT_FAST);
      if (draw_status != 0) {
        printf("epd_draw: failed\n");
      }
    }
  }

  if (close(display->fd) != 0) {
    printf("failed to close display fd whilst exiting\n");
  }

  free(display);

  return 0;
}
