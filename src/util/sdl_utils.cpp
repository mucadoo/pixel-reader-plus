#include "./sdl_utils.h"
#include "./sdl_font_cache.h"

#include <SDL/SDL_image.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "extern/stb/stb_image.h"

namespace
{

surface_unique_ptr surface_from_stb(const char *data, uint32_t size, SDL_PixelFormat *surface_format)
{
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char *pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc *>(data),
        static_cast<int>(size),
        &w,
        &h,
        &comp,
        4
    );
    if (!pixels || w <= 0 || h <= 0)
    {
        if (pixels)
        {
            stbi_image_free(pixels);
        }
        return nullptr;
    }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rmask = 0xff000000;
    Uint32 gmask = 0x00ff0000;
    Uint32 bmask = 0x0000ff00;
    Uint32 amask = 0x000000ff;
#else
    Uint32 rmask = 0x000000ff;
    Uint32 gmask = 0x0000ff00;
    Uint32 bmask = 0x00ff0000;
    Uint32 amask = 0xff000000;
#endif
    SDL_Surface *raw = SDL_CreateRGBSurfaceFrom(pixels, w, h, 32, w * 4, rmask, gmask, bmask, amask);
    if (!raw)
    {
        stbi_image_free(pixels);
        return nullptr;
    }
    SDL_Surface *converted = SDL_ConvertSurface(raw, surface_format, 0);
    SDL_FreeSurface(raw);
    stbi_image_free(pixels);
    return surface_unique_ptr { converted };
}

} // namespace

int detect_line_height(TTF_Font *font)
{
    int w, h;
    if (TTF_SizeUTF8(font, "A", &w, &h) == 0)
    {
        return h;
    }
    return 24;
}

int detect_line_height(const std::string &font, uint32_t size)
{
    return detect_line_height(cached_load_font(font, size));
}

surface_unique_ptr load_surface_from_ptr(const char *data, uint32_t size, const std::string &img_format, SDL_PixelFormat *surface_format)
{
    char type_str[8];
    {
        strncpy(type_str, img_format.c_str(), sizeof(type_str));
        type_str[sizeof(type_str) - 1] = 0;
        for (auto &c: type_str)
        {
            c = std::toupper(c);
        }
    }

    if (surface_unique_ptr decoded = surface_from_stb(data, size, surface_format))
    {
        return decoded;
    }

    SDL_RWops *rw = SDL_RWFromConstMem(data, size);
    auto loaded_surface = surface_unique_ptr { IMG_LoadTyped_RW(rw, 0, type_str) };
    SDL_FreeRW(rw);
    if (loaded_surface == nullptr)
    {
        std::cerr << "Failed to load image of type " << type_str << ": " << IMG_GetError() << std::endl;
        return nullptr;
    }

    return surface_unique_ptr { SDL_ConvertSurface(loaded_surface.get(), surface_format, 0) };
}
