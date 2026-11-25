#include "./progress_screen.h"

#include "./system_styling.h"
#include "./views/menu_draw.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <SDL/SDL.h>

#include <algorithm>

namespace progress_screen
{

void draw(SDL_Surface *dest, SystemStyling &styling, const std::string &title, const std::string &detail, int percent)
{
    const auto &theme = styling.get_loaded_color_theme();
    TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
    const SDL_Color quiet = menu_draw::quiet_text(theme);
    const auto color = [&](SDL_Color c) { return SDL_MapRGB(dest->format, c.r, c.g, c.b); };

    SDL_FillRect(dest, NULL, color(theme.background));

    auto head = menu_draw::shade(font, title, theme.main_text, theme.background);
    auto sub = menu_draw::shade(font, menu_draw::ellipsize(font, detail, SCREEN_WIDTH - 40), quiet, theme.background);
    auto value = percent >= 0
        ? menu_draw::shade(font, std::to_string(std::min(percent, 100)) + "%", quiet, theme.background)
        : nullptr;
    if (!head)
    {
        return;
    }

    const int gap = 8;
    const int bar_w = std::min(SCREEN_WIDTH - 80, 320);
    const int bar_h = 6;
    int total = head->h;
    if (sub)
    {
        total += head->h / 2 + sub->h;
    }
    if (percent >= 0)
    {
        total += gap + bar_h + (value ? gap + value->h : 0);
    }

    int y = SCREEN_HEIGHT / 2 - total / 2;
    SDL_Rect at = {static_cast<Sint16>(SCREEN_WIDTH / 2 - head->w / 2), static_cast<Sint16>(y), 0, 0};
    SDL_BlitSurface(head.get(), NULL, dest, &at);
    y += head->h;
    if (sub)
    {
        y += head->h / 2;
        SDL_Rect at_sub = {static_cast<Sint16>(SCREEN_WIDTH / 2 - sub->w / 2), static_cast<Sint16>(y), 0, 0};
        SDL_BlitSurface(sub.get(), NULL, dest, &at_sub);
        y += sub->h;
    }
    if (percent >= 0)
    {
        y += gap;
        SDL_Rect track = {static_cast<Sint16>(SCREEN_WIDTH / 2 - bar_w / 2), static_cast<Sint16>(y), static_cast<Uint16>(bar_w), static_cast<Uint16>(bar_h)};
        SDL_FillRect(dest, &track, color(quiet));
        SDL_Rect inner = {static_cast<Sint16>(track.x + 1), static_cast<Sint16>(track.y + 1), static_cast<Uint16>(bar_w - 2), static_cast<Uint16>(bar_h - 2)};
        SDL_FillRect(dest, &inner, color(theme.background));
        SDL_Rect fill = {inner.x, inner.y, static_cast<Uint16>(inner.w * std::min(percent, 100) / 100), inner.h};
        SDL_FillRect(dest, &fill, color(theme.main_text));
        y += bar_h;
        if (value)
        {
            y += gap;
            SDL_Rect at_value = {static_cast<Sint16>(SCREEN_WIDTH / 2 - value->w / 2), static_cast<Sint16>(y), 0, 0};
            SDL_BlitSurface(value.get(), NULL, dest, &at_value);
        }
    }
}

void show(SystemStyling &styling, const std::string &title, const std::string &detail, int percent)
{
    static std::string last_title;
    static std::string last_detail;
    static int last_percent = -2;
    if (percent == last_percent && title == last_title && detail == last_detail)
    {
        return;
    }
    last_title = title;
    last_detail = detail;
    last_percent = percent;

    SDL_Surface *video = SDL_GetVideoSurface();
    if (!video)
    {
        return;
    }
    draw(video, styling, title, detail, percent);
    SDL_Flip(video);
}

} // namespace progress_screen
