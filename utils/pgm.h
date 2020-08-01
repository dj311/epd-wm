#ifndef PGM_H
#define PGM_H

#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>


char next_non_whitespace(
  FILE * fd
);


typedef struct
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel;
  unsigned char *pixels;
} pgm;


pgm *pgm_load(
  char path[]
);

int pgm_print(
  pgm * image
);

pgm *pgm_generate_solid_color(
  unsigned int color,
  unsigned int width,
  unsigned int height
);

pgm *pgm_generate_gradient(
  unsigned int width,
  unsigned int height
);

int pgm_filter_two_bit(
  pgm * image
);

int pgm_filter_one_bit(
  pgm * image
);


#endif
