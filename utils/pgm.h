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


#endif
