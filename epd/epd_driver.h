#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H


#include<utils/pgm.h>


/* SCSI Generic ----------------------------------------------------------------
*/
typedef unsigned char sg_command;
typedef unsigned char sg_data;


// SCSI operation codes (used as the first byte of an CommandDescriptorBlock).
enum sg_opcode
{
  SG_OP_INQUIRY = 0x12,
  SG_OP_CUSTOM = 0xFE
};


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
  EPD_UPD_EIGHT_BIT_FAST = 4,   // this isn't 8 bit, maybe two bit fast?
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


typedef struct
{
  unsigned char sg_op;
  unsigned char zero0;
  unsigned int address;
  unsigned char epd_op;
  unsigned short length;
  unsigned char zero1;
  unsigned char zero2;
  unsigned char zero3;
  unsigned char zero4;
  unsigned char zero5;
  unsigned char zero6;
  unsigned char zero7;
} __attribute__((__packed__)) epd_fast_write_command;


int epd_fast_write_mem(
  epd * display,
  pgm * image
);


int epd_transfer_image(
  epd * display,
  unsigned int x,
  unsigned int y,
  pgm * image
);


int epd_draw(
  epd * display,
  unsigned int x,
  unsigned int y,
  pgm * image,
  enum epd_update_mode update_mode
);


int epd_reset(
  epd * display
);


epd *epd_init(
  char path[]
);


#endif
