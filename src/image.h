#ifndef IMAGE_H
#define IMAGE_H

#include <SDL.h>
#include <SDL_ttf.h>

#include "gl.h"

/*---------------------------------------------------------------------------*/

void   image_size(int *, int *, int, int);

GLuint make_image_from_surf(int *, int *, SDL_Surface *);
GLuint make_image_from_file(int *, int *, const char *);
GLuint make_image_from_font(int *, int *,
                            int *, int *, const char *, TTF_Font *);

/*---------------------------------------------------------------------------*/

#endif
