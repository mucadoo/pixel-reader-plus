#ifndef SCREEN_H_
#define SCREEN_H_

#include <SDL/SDL_keysym.h>

// The size of the screen as the reader draws it. In portrait this is the display turned on its side.
extern short unsigned int SCREEN_WIDTH;
extern short unsigned int SCREEN_HEIGHT;

struct SDL_PixelFormat;
struct SDL_Surface;
SDL_PixelFormat *get_render_surface_format();
void set_render_surface_format(SDL_PixelFormat *format);

// 0 is the display as it is. 1 turns the top of the page to the right edge of the display, 2 to the left edge.
int screen_rotation();

// Call once, after SCREEN_WIDTH and SCREEN_HEIGHT hold the size of the display and before any surface is made.
// For a turned screen it swaps them.
void set_screen_rotation(int rotation);

// Copies the page that was drawn at SCREEN_WIDTH x SCREEN_HEIGHT onto the display, turned to match.
void present_screen(SDL_Surface *page, SDL_Surface *display);

// The D-pad key as the user sees it for a key as the device reports it, and back. They are the same on an
// unturned screen.
SDLKey screen_key_from_device(SDLKey device_key);
SDLKey device_key_from_screen(SDLKey screen_key);

#endif