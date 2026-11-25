#ifndef PROGRESS_SCREEN_H_
#define PROGRESS_SCREEN_H_

#include <SDL/SDL_video.h>

#include <string>

struct SystemStyling;

// A full-screen notice with a bar, for work that has to finish before the
// reader can go on, so the screen never just freezes.
namespace progress_screen
{

// percent below 0 draws no bar. detail may be empty.
void draw(SDL_Surface *dest, SystemStyling &styling, const std::string &title, const std::string &detail, int percent);

// Draws straight to the display, for work that holds up the main loop.
// Does nothing when the percent and text are the same as the last call.
void show(SystemStyling &styling, const std::string &title, const std::string &detail, int percent);

} // namespace progress_screen

#endif
