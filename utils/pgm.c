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


char
next_non_whitespace(
  FILE * fd
)
{
  char character = getc(fd);
  while (isspace(character) != 0) {
    character = getc(fd);
  }
  return character;
}


pgm *
pgm_load(
  char path[]
)
{
  /* Open file for reading */
  FILE *fd = fopen(path, "r");
  if (fd == NULL) {
    printf("Failed to open PGM file.\n");
    return -1;
  }

  /* Read in image parameters */
  char magic_number[3];
  magic_number[0] = getc(fd);
  magic_number[1] = getc(fd);
  magic_number[2] = NULL;
  if (strcmp(magic_number, "P5\0") != 0) {
    printf(magic_number);
    printf("Image at path given is not a PGM file.\n");
    return -1;
  }

  char character = next_non_whitespace(fd);

  int index = 0;
  char width_str[100] = { 0 };
  while ((isspace(character) == 0) && (character != EOF)) {
    width_str[index] = character;
    character = getc(fd);
    index++;
  }
  width_str[index] = NULL;

  character = next_non_whitespace(fd);

  index = 0;
  char height_str[100] = { 0 };
  while ((isspace(character) == 0) && (character != EOF)) {
    height_str[index] = character;
    character = getc(fd);
    index++;
  }
  height_str[index] = NULL;

  character = next_non_whitespace(fd);

  index = 0;
  char max_gray_str[100] = { 0 };
  while ((isspace(character) == 0) && (character != EOF)) {
    max_gray_str[index] = character;
    character = getc(fd);
    index++;
  }
  max_gray_str[index] = NULL;

  int width = atoi(width_str);
  int height = atoi(height_str);
  int max_gray = atoi(max_gray_str);

  int bytes_per_pixel;
  if (max_gray < 256) {
    bytes_per_pixel = 1;
  } else {
    bytes_per_pixel = 2;
  }

  printf("Loading PGM:\n");
  printf("\twidth=%d, height=%d, max_gray=%d, bytes_per_pixel=%d\n", width,
         height, max_gray, bytes_per_pixel);

  /* Allocate appropriately sized memory */
  int num_bytes = width * height * bytes_per_pixel;
  unsigned char *bytes = malloc(sizeof(unsigned char) * num_bytes);

  character = next_non_whitespace(fd);

  bytes[0] = character;
  for (index = 1; index < num_bytes; index++) {
    character = getc(fd);
    bytes[index] = (unsigned char) character;
  }

  pgm *image = malloc(sizeof(pgm));
  image->width = width;
  image->height = height;
  image->bytes_per_pixel = bytes_per_pixel;
  image->pixels = bytes;

  fclose(fd);
  return image;
}

int
pgm_print(
  pgm * image
)
{
  char greyscale[] = " .:-=+*#%@";

  unsigned int x, y, index;
  for (x = 0; x < image->width; x++) {
    for (y = 0; y < image->height; y++) {
      index = x + y * image->width;
      if (image->pixels[index] < 30) {
        putchar(greyscale[9]);
        continue;
      }
      if (image->pixels[index] < 60) {
        putchar(greyscale[8]);
        continue;
      }
      if (image->pixels[index] < 90) {
        putchar(greyscale[7]);
        continue;
      }
      if (image->pixels[index] < 120) {
        putchar(greyscale[6]);
        continue;
      }
      if (image->pixels[index] < 150) {
        putchar(greyscale[5]);
        continue;
      }
      if (image->pixels[index] < 180) {
        putchar(greyscale[4]);
        continue;
      }
      if (image->pixels[index] < 210) {
        putchar(greyscale[3]);
        continue;
      }
      if (image->pixels[index] < 240) {
        putchar(greyscale[2]);
        continue;
      }
      if (image->pixels[index] < 255) {
        putchar(greyscale[1]);
        continue;
      }
      if (image->pixels[index] == 255) {
        putchar(greyscale[0]);
        continue;
      }
    }
    printf("\n");
  }

  return 0;
}

pgm *
pgm_generate(
  unsigned int width,
  unsigned int height
)
{
  unsigned int num_pixels = width * height;
  unsigned char *pixels = malloc(sizeof(unsigned char) * num_pixels);

  for (unsigned int x = 0; x < width; x++) {
    for (unsigned int y = 0; y < height; y++) {
      pixels[y * width + x] = 256 * (y * width + x) / num_pixels;
    }
  }

  pgm *image = malloc(sizeof(pgm));
  image->width = width;
  image->height = height;
  image->bytes_per_pixel = 1;
  image->pixels = pixels;

  return image;
}
