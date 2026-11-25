#ifndef PICTURE_FILTER_H_
#define PICTURE_FILTER_H_

#include <SDL/SDL_video.h>

// How pictures in a book are shown.
enum PictureMode
{
    PICTURES_NORMAL = 0,
    PICTURES_INVERTED = 1,      // light and dark swapped, for diagrams and scans on a dark theme
    PICTURES_HIGH_CONTRAST = 2, // darks darker and lights lighter, for faint scans
    PICTURES_HIDDEN = 3,
    PICTURE_MODE_COUNT = 4
};

const char *picture_mode_label(int mode);

// Changes the picture in place. Only 32 bit surfaces are touched; others stay as they are.
void apply_picture_filter(SDL_Surface *surface, int mode);

#endif
