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


// scsi generic ------------------------------------------------------------- //
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


// it8951 epd driver -------------------------------------------------------- //

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
  struct epd_info info;
};


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
    0xFE, 0x00, 0x38, 0x39, 0x35, 0x31, 0x80, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00
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
