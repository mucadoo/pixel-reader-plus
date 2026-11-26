#include "./screen_dim.h"

#include <algorithm>
#include <cstdint>

namespace screen_dim
{

namespace
{
int g_level = 0;
}

int percent_for_level(int level)
{
    static const int values[LEVEL_COUNT] = {0, 25, 50, 70};
    return values[std::clamp(level, 0, LEVEL_COUNT - 1)];
}

const char *label_for_level(int level)
{
    static const char *labels[LEVEL_COUNT] = {"Off", "Dim", "Dimmer", "Dimmest"};
    return labels[std::clamp(level, 0, LEVEL_COUNT - 1)];
}

void set_level(int level)
{
    g_level = std::clamp(level, 0, LEVEL_COUNT - 1);
}

void apply(SDL_Surface *surface)
{
    if (g_level == 0 || !surface || surface->format->BytesPerPixel != 4)
    {
        return;
    }
    const uint32_t keep = static_cast<uint32_t>(256 - percent_for_level(g_level) * 256 / 100); // out of 256
    const bool lock = SDL_MUSTLOCK(surface);
    if (lock && SDL_LockSurface(surface) != 0)
    {
        return;
    }
    const SDL_PixelFormat *fmt = surface->format;
    const uint32_t rmask = fmt->Rmask;
    const uint32_t gmask = fmt->Gmask;
    const uint32_t bmask = fmt->Bmask;
    for (int y = 0; y < surface->h; ++y)
    {
        uint32_t *row = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(surface->pixels) + static_cast<size_t>(y) * surface->pitch);
        for (int x = 0; x < surface->w; ++x)
        {
            const uint32_t p = row[x];
            const uint32_t r = (((p & rmask) >> fmt->Rshift) * keep) >> 8;
            const uint32_t g = (((p & gmask) >> fmt->Gshift) * keep) >> 8;
            const uint32_t b = (((p & bmask) >> fmt->Bshift) * keep) >> 8;
            row[x] = (p & ~(rmask | gmask | bmask)) | (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift);
        }
    }
    if (lock)
    {
        SDL_UnlockSurface(surface);
    }
}

} // namespace screen_dim
