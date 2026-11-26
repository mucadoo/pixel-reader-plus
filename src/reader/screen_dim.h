#ifndef SCREEN_DIM_H_
#define SCREEN_DIM_H_

#include <SDL/SDL_video.h>

// A software dimmer for night reading: the backlight of the device has a lowest step that can
// still be too bright, so the picture itself is darkened a little more.
namespace screen_dim
{

constexpr int LEVEL_COUNT = 4;

// 0 is off, then 25%, 50% and 70% darker.
int percent_for_level(int level);
const char *label_for_level(int level);

void set_level(int level);

// Darkens the surface about to be shown. Does nothing at level 0.
void apply(SDL_Surface *surface);

} // namespace screen_dim

#endif
