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

#include<wlr/util/log.h>
#include<epd/epd_driver.h>
#include<utils/pgm.h>


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
  message_pointer->timeout = 0;
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
    wlr_log(WLR_INFO, "send_message: failed with status %i", status);
  }

  free(message_pointer);

  return status;
}


/* IT8951 EPD Driver -----------------------------------------------------------

*/


int
epd_fast_write_mem(
  epd * display,
  pgm * image
)
{
  /* Broken. TODO: Fix? It would be nice to be able to send a full
     updates worth of pixels in one round trip. */

  if (display->state != EPD_READY) {
    wlr_log(WLR_INFO,
            "epd_fast_write_mem: display must be in EPD_READY state");
    return -1;
  }

  unsigned int fw_data_length = image->width * image->height;

  epd_fast_write_command fw_command;
  memset(&fw_command, 0, sizeof(epd_fast_write_command));
  fw_command.sg_op = SG_OP_CUSTOM;
  fw_command.address = display->info.image_buffer_address;
  fw_command.epd_op = EPD_OP_FAST_WRITE_MEM;
  fw_command.length = htonl(fw_data_length);

  int fw_status = send_message(display->fd,
                               16,
                               (sg_command *) & fw_command,
                               SG_DXFER_TO_DEV,
                               fw_data_length,
                               (sg_data *) image->pixels);

  if (fw_status != 0) {
    wlr_log(WLR_INFO, "epd_fast_write_mem: failed to write to memory");
    wlr_log(WLR_INFO, "%lu", sizeof(epd_fast_write_command));
    wlr_log(WLR_INFO, "%u", fw_data_length);
    return -1;
  }

  sg_command dpy_command[16] = {
    SG_OP_CUSTOM, 0, 0, 0, 0, 0, EPD_OP_DPY_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  epd_display_area_args_addr dpy_data;
  dpy_data.address = display->info.image_buffer_address;
  dpy_data.update_mode = EPD_UPD_GLR16;
  dpy_data.x = 0;
  dpy_data.y = 0;
  dpy_data.width = display->info.width;
  dpy_data.height = display->info.height;
  dpy_data.wait_display_ready = 0;

  int status = send_message(display->fd,
                            16,
                            dpy_command,
                            SG_DXFER_TO_DEV,
                            sizeof(epd_display_area_args_addr),
                            (sg_data *) & dpy_data);

  if (status != 0) {
    wlr_log(WLR_INFO, "epd_fast_write_mem: dispaly command failed");
    return -1;
  }

  wlr_log(WLR_INFO, "epd_fast_write_mem: success");

  return 0;
}


int
epd_transfer_image(
  epd * display,
  unsigned int x,
  unsigned int y,
  unsigned int width,
  unsigned int height,
  unsigned char *pixels
)
{

  // Transfer the image given in `image` into the image buffer in
  // `epd`s memory, at the offset determined by `x` and `y`.

  // Chunked based on the max transfer size of the display, taking
  // whole rows of the input image (under the assumption that
  // image->width < epd->max_tranfer).

  unsigned int image_address_le = ntohl(display->info.image_buffer_address);

  unsigned int max_chunk_height = display->max_transfer / width;

  unsigned int start_row, end_row;
  unsigned int chunk_width, chunk_height;

  wlr_log(WLR_INFO, "epd_transfer_image: %u %u %u %u %u %u",
          display->max_transfer, max_chunk_height, x, y, width, height);

  for (start_row = 0; start_row < height; start_row += max_chunk_height) {
    wlr_log(WLR_INFO, "epd_transfer_image: start_row=%u", start_row);

    end_row = start_row + max_chunk_height;
    if (end_row > height) {
      end_row = height;
    }

    chunk_width = width;
    chunk_height = end_row - start_row;

    unsigned long chunk_address_in_src_image = 0 + start_row * chunk_width;
    unsigned long chunk_address_in_our_memory =
      pixels + chunk_address_in_src_image;

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
      wlr_log(WLR_INFO,
              "epd_transfer_image: failed to send chunk %u so gave up",
              start_row);
      return -1;
    }

  }

  wlr_log(WLR_INFO, "epd_transfer_image: complete");
  return 0;
}


int
epd_draw_pgm(
  epd * display,
  unsigned int x,
  unsigned int y,
  pgm * image,
  enum epd_update_mode update_mode
)
{
  if (image->bytes_per_pixel != 1) {
    wlr_log(WLR_INFO, "epd_draw: only supports images with 1 byte per pixel");
    return -1;
  }
  return epd_draw(display, x, y, image->width, image->height, image->pixels,
                  update_mode);
}


int
epd_draw(
  epd * display,
  unsigned int x,
  unsigned int y,
  unsigned int width,
  unsigned int height,
  unsigned char *pixels,
  enum epd_update_mode update_mode
)
{
  if (display->state != EPD_READY) {
    wlr_log(WLR_INFO, "epd_draw: display must be in EPD_READY state");
    return -1;
  }

  if (x + width > display->info.width || y + height > display->info.height) {
    wlr_log(WLR_INFO,
            "epd_draw: cannot draw image outside of display boundary");
    return -1;
  }

  if (x == 0 && y == 0
      && width == display->info.width && height == display->info.height) {
    wlr_log(WLR_INFO, "epd_draw: detected full image update");
    // TODO: Can we optimise this case? I believe there are special ops we can do.
  }

  int transfer_success =
    epd_transfer_image(display, x, y, width, height, pixels);
  if (transfer_success != 0) {
    wlr_log(WLR_INFO, "epd_draw: failed to transfer image to device");
    return -1;
  }
  wlr_log(WLR_INFO, "epd_draw: transfer success");

  sg_command draw_command[16] = {
    SG_OP_CUSTOM, 0, 0, 0, 0, 0, EPD_OP_DPY_AREA, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  epd_display_area_args_addr draw_data;
  draw_data.address = display->info.image_buffer_address;
  draw_data.update_mode = htonl(update_mode);
  draw_data.x = htonl(x);
  draw_data.y = htonl(y);
  draw_data.width = htonl(width);
  draw_data.height = htonl(height);
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
  wlr_log(WLR_INFO, "epd_draw: draw success");

  return 0;

}


int
epd_reset(
  epd * display
)
{
  if (display->state != EPD_READY) {
    wlr_log(WLR_INFO, "epd_reset: display must be in EPD_READY state");
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

  wlr_log(WLR_INFO, "epd_reset: success");

  return 0;
}


int
epd_set_vcom(
  epd * display,
  unsigned int voltage
)
{
  /*
     Setting the VCOM calibrates the IT8951 for a particular
     display. Each display has a number printed on it's FPC connector
     (mine is on a white label with a QR code on it). This number is a
     voltage and should be negative. On my display it says "-1.81"
     (volts).

     This function takes the vcom parameter in the form of an unsigned
     int. This should the value printed on your display, made +ve and
     measured in millivolts, i.e. -1000 * <value-on-board>. For
     example, I would use -1000*-1.81=1810 for the display mentioned
     above.
   */

  if (display->state != EPD_INIT) {
    return -1;
  }

  epd_set_vcom_command vcom_command;
  memset(&vcom_command, 0, sizeof(epd_set_vcom_command));
  vcom_command.sg_op = SG_OP_CUSTOM;
  vcom_command.epd_op = EPD_OP_PMIC_CTRL;
  vcom_command.set_vcom = 1;
  vcom_command.vcom_value = htonl(voltage);

  int status = send_message(display->fd,
                            sizeof(epd_set_vcom_command),
                            (sg_command *) & vcom_command,
                            SG_DXFER_TO_DEV,
                            0,
                            (sg_data *) NULL);

  if (status != 0) {
    return -1;
  }

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
    wlr_log(WLR_INFO,
            "epd_ensure_it8951_display: display struct not in EPD_INIT state");
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
    wlr_log(WLR_INFO, "epd_ensure_it8951_display: inquiry msg failed");
    return -1;
  }

  unsigned char *device_name = malloc(28);
  memcpy(device_name, (unsigned char *) (inquiry_response + 8), 28);

  unsigned char *expected_device_name = malloc(28);
  expected_device_name = (unsigned char *) "Generic Storage RamDisc 1.00";

  if (memcmp(device_name, expected_device_name, 28) != 0) {
    wlr_log(WLR_INFO,
            "epd_ensure_it8951_display: name doesn't match (device_name=%28s)",
            device_name);
    free(device_name);
    return -1;
  }


  free(device_name);
  return 0;
}


int
epd_init(
  epd * display,
  char path[],
  unsigned int vcom_voltage
)
{
  wlr_log(WLR_INFO, "epd_init:");
  wlr_log(WLR_INFO, "epd_init: %s, %u", path, vcom_voltage);

  display->fd = open(path, O_RDWR);
  display->state = EPD_INIT;
  display->max_transfer = 60000;

  wlr_log(WLR_INFO, "epd_init: ensure we are talking to an it8951 on path %s",
          path);
  if (epd_ensure_it8951_display(display) != 0) {
    free(display);
    return -1;
  }

  wlr_log(WLR_INFO, "epd_init: getting display information");
  epd_get_system_info(display);

  wlr_log(WLR_INFO, "epd_init: width=%u, height=%u",
          ntohl(display->info.width), ntohl(display->info.height));

  if (epd_set_vcom(display, vcom_voltage) != 0) {
    free(display);
    return -1;
  }

  wlr_log(WLR_INFO, "epd_init: complete - display ready");
  display->state = EPD_READY;

  return 0;
}
