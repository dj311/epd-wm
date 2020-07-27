#ifndef PGM_H
#define PGM_H


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

pgm *pgm_solid_color(
  unsigned int color,
  unsigned int width,
  unsigned int height
);

pgm *pgm_generate(
  unsigned int width,
  unsigned int height
);

int pgm_two_bit(
  pgm * image
);

int pgm_one_bit(
  pgm * image
);


#endif
