#include<fcntl.h>
#include<unistd.h>
#include<scsi/scsi_ioctl.h>
#include<scsi/sg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>


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

int
init_display(
  char path[]
)
{
  int display_fd = open(path, O_RDWR);

  unsigned char inquiry_response[40] = { 0 };
  unsigned char inquiry_command[16] =
    { 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int status =
    send_message(display_fd, 16, &inquiry_command, SG_DXFER_FROM_DEV, 40,
                 &inquiry_response);

  if (status != 0)
    {
      return status;
    }

  char *device_name = malloc(28);
  strncpy(device_name, inquiry_response + 8, 28);

  if (strcmp(device_name, "Generic Storage RamDisc 1.00") == 0)
    {
      return 0;
    }

  return -1;
}


// wlroots interface -------------------------------------------------------- //



// demo --------------------------------------------------------------------- //

int
main(
)
{
  printf("Hello, Dan!\n");
  return init_display("/dev/sg1");
}
