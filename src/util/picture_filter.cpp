#include "./picture_filter.h"

#include <SDL/SDL.h>

#include <algorithm>

const char *picture_mode_label(int mode)
{
    switch (mode)
    {
        case PICTURES_INVERTED: return "Inverted";
        case PICTURES_HIGH_CONTRAST: return "High contrast";
        case PICTURES_HIDDEN: return "Hidden";
        default: return "Normal";
    }
}

void apply_picture_filter(SDL_Surface *surface, int mode)
{
    if (!surface || (mode != PICTURES_INVERTED && mode != PICTURES_HIGH_CONTRAST)
        || !surface->format || surface->format->BytesPerPixel != 4)
    {
        return;
    }
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
    {
        return;
    }
    auto contrast = [](Uint8 value) {
        const int boosted = (static_cast<int>(value) - 128) * 3 / 2 + 128;
        return static_cast<Uint8>(std::clamp(boosted, 0, 255));
    };
    for (int y = 0; y < surface->h; ++y)
    {
        Uint32 *row = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(surface->pixels) + y * surface->pitch);
        for (int x = 0; x < surface->w; ++x)
        {
            Uint8 r, g, b, a;
            SDL_GetRGBA(row[x], surface->format, &r, &g, &b, &a);
            if (mode == PICTURES_INVERTED)
            {
                r = static_cast<Uint8>(255 - r);
                g = static_cast<Uint8>(255 - g);
                b = static_cast<Uint8>(255 - b);
            }
            else
            {
                r = contrast(r);
                g = contrast(g);
                b = contrast(b);
            }
            row[x] = SDL_MapRGBA(surface->format, r, g, b, a);
        }
    }
    if (SDL_MUSTLOCK(surface))
    {
        SDL_UnlockSurface(surface);
    }
}
