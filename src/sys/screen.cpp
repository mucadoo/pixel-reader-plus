#include "./screen.h"

#include "./keymap.h"

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>
#include <stdexcept>

static bool format_initialized = false;
static SDL_PixelFormat format;
short unsigned int SCREEN_WIDTH  = 640;
short unsigned int SCREEN_HEIGHT = 480;

static int g_rotation = 0;
static SDL_Surface *g_turned = nullptr;

SDL_PixelFormat *get_render_surface_format()
{
    if (!format_initialized)
    {
        throw std::runtime_error("Render surface format not initialized");
    }
    return &format;
}

void set_render_surface_format(SDL_PixelFormat *new_format)
{
    format_initialized = true;
    format = *new_format;
}

int screen_rotation()
{
    return g_rotation;
}

void set_screen_rotation(int rotation)
{
    g_rotation = (rotation == 1 || rotation == 2) ? rotation : 0;
    if (g_rotation != 0)
    {
        const short unsigned int width = SCREEN_WIDTH;
        SCREEN_WIDTH = SCREEN_HEIGHT;
        SCREEN_HEIGHT = width;
    }
}

void present_screen(SDL_Surface *page, SDL_Surface *display)
{
    if (!page || !display)
    {
        return;
    }
    if (g_rotation == 0)
    {
        SDL_BlitSurface(page, NULL, display, NULL);
        return;
    }

    const int page_w = page->w;
    const int page_h = page->h;
    if (!g_turned || g_turned->w != page_h || g_turned->h != page_w)
    {
        if (g_turned)
        {
            SDL_FreeSurface(g_turned);
        }
        g_turned = SDL_CreateRGBSurface(
            SDL_SWSURFACE, page_h, page_w, 32,
            page->format->Rmask, page->format->Gmask, page->format->Bmask, page->format->Amask
        );
        if (!g_turned)
        {
            return;
        }
    }

    SDL_LockSurface(g_turned);
    const bool lock_page = SDL_MUSTLOCK(page);
    if (lock_page)
    {
        SDL_LockSurface(page);
    }
    const int from_pitch = page->pitch / 4;
    const int to_pitch = g_turned->pitch / 4;
    const Uint32 *from = static_cast<const Uint32 *>(page->pixels);
    Uint32 *to = static_cast<Uint32 *>(g_turned->pixels);
    if (g_rotation == 1)
    {
        // The top of the page goes to the right edge: a row of the page becomes a column.
        for (int y = 0; y < page_h; ++y)
        {
            const Uint32 *row = from + y * from_pitch;
            Uint32 *column = to + (page_h - 1 - y);
            for (int x = 0; x < page_w; ++x)
            {
                column[x * to_pitch] = row[x];
            }
        }
    }
    else
    {
        // The top of the page goes to the left edge.
        for (int y = 0; y < page_h; ++y)
        {
            const Uint32 *row = from + y * from_pitch;
            Uint32 *column = to + y;
            for (int x = 0; x < page_w; ++x)
            {
                column[(page_w - 1 - x) * to_pitch] = row[x];
            }
        }
    }
    if (lock_page)
    {
        SDL_UnlockSurface(page);
    }
    SDL_UnlockSurface(g_turned);

    SDL_BlitSurface(g_turned, NULL, display, NULL);
}

// Rotation 1 has the display's right edge as the top of the page, so the device's right arrow points up.
// Rotation 2 is the other way around.
SDLKey screen_key_from_device(SDLKey key)
{
    if (g_rotation == 1)
    {
        if (key == SW_BTN_RIGHT) return SW_BTN_UP;
        if (key == SW_BTN_UP) return SW_BTN_LEFT;
        if (key == SW_BTN_LEFT) return SW_BTN_DOWN;
        if (key == SW_BTN_DOWN) return SW_BTN_RIGHT;
    }
    else if (g_rotation == 2)
    {
        if (key == SW_BTN_LEFT) return SW_BTN_UP;
        if (key == SW_BTN_UP) return SW_BTN_RIGHT;
        if (key == SW_BTN_RIGHT) return SW_BTN_DOWN;
        if (key == SW_BTN_DOWN) return SW_BTN_LEFT;
    }
    return key;
}

SDLKey device_key_from_screen(SDLKey key)
{
    if (g_rotation == 1)
    {
        if (key == SW_BTN_UP) return SW_BTN_RIGHT;
        if (key == SW_BTN_LEFT) return SW_BTN_UP;
        if (key == SW_BTN_DOWN) return SW_BTN_LEFT;
        if (key == SW_BTN_RIGHT) return SW_BTN_DOWN;
    }
    else if (g_rotation == 2)
    {
        if (key == SW_BTN_UP) return SW_BTN_LEFT;
        if (key == SW_BTN_RIGHT) return SW_BTN_UP;
        if (key == SW_BTN_DOWN) return SW_BTN_RIGHT;
        if (key == SW_BTN_LEFT) return SW_BTN_DOWN;
    }
    return key;
}