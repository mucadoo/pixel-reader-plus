#include "./image_view.h"

#include "./menu_draw.h"
#include "extern/rotozoom/SDL_rotozoom.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <SDL/SDL.h>

#include <algorithm>
#include <cstdio>

namespace
{

constexpr double ZOOM_STEPS[] = {1.0, 1.5, 2.0, 3.0, 4.0};
constexpr int ZOOM_STEP_COUNT = 5;
constexpr double MAX_FIT_UPSCALE = 3.0;
constexpr double MAX_PIXELS = 8.0 * 1000 * 1000; // keeps the zoomed copy to about 32 MB
constexpr int PAN_STEP = 28;
const int SCREEN_W = static_cast<int>(::SCREEN_WIDTH);
const int SCREEN_H = static_cast<int>(::SCREEN_HEIGHT);

} // namespace

ImageView::ImageView(const std::vector<char> &data, const std::string &extension, SystemStyling &styling)
    : styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , pan_throttle(250, 30)
{
    if (!data.empty())
    {
        original = load_surface_from_ptr(data.data(), static_cast<uint32_t>(data.size()), extension, get_render_surface_format());
    }
    if (original && original->w > 0 && original->h > 0)
    {
        fit = std::min(
            static_cast<double>(SCREEN_W) / original->w,
            static_cast<double>(SCREEN_H) / original->h
        );
        fit = std::min(fit, MAX_FIT_UPSCALE);
        set_zoom(0);
    }
    else
    {
        original.reset();
    }
}

ImageView::~ImageView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

SDL_Surface *ImageView::current() const
{
    return scaled ? scaled.get() : original.get();
}

void ImageView::clamp_offsets()
{
    SDL_Surface *surface = current();
    if (!surface)
    {
        return;
    }
    offset_x = std::clamp(offset_x, 0, std::max(0, surface->w - SCREEN_W));
    offset_y = std::clamp(offset_y, 0, std::max(0, surface->h - SCREEN_H));
}

bool ImageView::set_zoom(int step)
{
    if (!original)
    {
        return false;
    }
    step = std::clamp(step, 0, ZOOM_STEP_COUNT - 1);
    const double scale = fit * ZOOM_STEPS[step];
    const double pixels = static_cast<double>(original->w) * original->h * scale * scale;
    if (step > 0 && pixels > MAX_PIXELS)
    {
        return false; // too big to hold
    }

    // Keep the middle of the screen over the same part of the picture.
    SDL_Surface *before = current();
    double center_x = 0.5;
    double center_y = 0.5;
    if (before && before->w > 0 && before->h > 0 && scaled)
    {
        center_x = std::min(1.0, (offset_x + std::min(SCREEN_W, before->w) / 2.0) / before->w);
        center_y = std::min(1.0, (offset_y + std::min(SCREEN_H, before->h) / 2.0) / before->h);
    }

    surface_unique_ptr next { zoomSurface(original.get(), scale, scale, 1) };
    if (!next)
    {
        return false;
    }
    scaled = std::move(next);
    zoom_step = step;
    offset_x = static_cast<int>(center_x * scaled->w) - SCREEN_W / 2;
    offset_y = static_cast<int>(center_y * scaled->h) - SCREEN_H / 2;
    clamp_offsets();
    _needs_render = true;
    return true;
}

void ImageView::pan(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT: offset_x -= PAN_STEP; break;
        case SW_BTN_RIGHT: offset_x += PAN_STEP; break;
        case SW_BTN_UP: offset_y -= PAN_STEP; break;
        case SW_BTN_DOWN: offset_y += PAN_STEP; break;
        default: return;
    }
    clamp_offsets();
    _needs_render = true;
}

bool ImageView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    const auto &theme = styling.get_loaded_color_theme();
    SDL_FillRect(dest_surface, nullptr, SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b));

    SDL_Surface *surface = current();
    if (!surface)
    {
        return true;
    }
    const int draw_w = std::min(surface->w, SCREEN_W);
    const int draw_h = std::min(surface->h, SCREEN_H);
    SDL_Rect src = {
        static_cast<Sint16>(surface->w > SCREEN_W ? offset_x : 0),
        static_cast<Sint16>(surface->h > SCREEN_H ? offset_y : 0),
        static_cast<Uint16>(draw_w),
        static_cast<Uint16>(draw_h)
    };
    SDL_Rect dst = {
        static_cast<Sint16>((SCREEN_W - draw_w) / 2),
        static_cast<Sint16>((SCREEN_H - draw_h) / 2),
        0,
        0
    };
    SDL_BlitSurface(surface, &src, dest_surface, &dst);

    if (hint_ms_left > 0)
    {
        TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
        if (font)
        {
            char zoom_text[24];
            std::snprintf(zoom_text, sizeof(zoom_text), "%d%%", static_cast<int>(ZOOM_STEPS[zoom_step] * 100 + 0.5));
            const std::string text = std::string(zoom_text) + "   A  Zoom   Y  Fit   B  Close";
            if (auto line = menu_draw::shade(font, text, theme.main_text, theme.background))
            {
                SDL_Rect box = {0, static_cast<Sint16>(SCREEN_H - line->h - 6), static_cast<Uint16>(SCREEN_W), static_cast<Uint16>(line->h + 6)};
                SDL_FillRect(dest_surface, &box, SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b));
                SDL_Rect at = {static_cast<Sint16>(SCREEN_W / 2 - line->w / 2), static_cast<Sint16>(box.y + 3), 0, 0};
                SDL_BlitSurface(line.get(), nullptr, dest_surface, &at);
            }
        }
    }
    return true;
}

bool ImageView::is_done()
{
    return _is_done;
}

bool ImageView::is_modal()
{
    return false;
}

bool ImageView::tick(uint32_t elapsed_ms)
{
    if (hint_ms_left > 0)
    {
        hint_ms_left -= static_cast<int>(elapsed_ms);
        if (hint_ms_left <= 0)
        {
            hint_ms_left = 0;
            return true;
        }
    }
    return false;
}

void ImageView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
        case SW_BTN_UP:
        case SW_BTN_DOWN:
            pan(key);
            break;
        case SW_BTN_A:
        case SW_BTN_R1:
            hint_ms_left = 2000;
        {
            // The next zoom that fits in memory, else back to the whole picture.
            bool changed = false;
            for (int step = zoom_step + 1; step < ZOOM_STEP_COUNT && !changed; ++step)
            {
                changed = set_zoom(step);
            }
            if (!changed)
            {
                set_zoom(0);
            }
            break;
        }
        case SW_BTN_L1:
            hint_ms_left = 2000;
            set_zoom(zoom_step > 0 ? zoom_step - 1 : 0);
            break;
        case SW_BTN_Y:
            hint_ms_left = 2000;
            set_zoom(0);
            break;
        case SW_BTN_B:
        case SW_BTN_MENU:
        case SW_BTN_X:
            _is_done = true;
            return;
        default:
            return;
    }
    _needs_render = true;
}

void ImageView::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    switch (key)
    {
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
        case SW_BTN_UP:
        case SW_BTN_DOWN:
            if (pan_throttle(held_time_ms))
            {
                pan(key);
            }
            break;
        default:
            break;
    }
}
