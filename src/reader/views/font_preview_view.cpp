#include "./font_preview_view.h"

#include "./menu_draw.h"
#include "reader/config.h"
#include "reader/font_catalog.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <SDL/SDL.h>

#include <algorithm>
#include <filesystem>

namespace
{

const int SCREEN_W = static_cast<int>(::SCREEN_WIDTH);
const int SCREEN_H = static_cast<int>(::SCREEN_HEIGHT);

const char *SAMPLE =
    "It was a bright cold day in April, and the clocks were striking thirteen. "
    "\"Where have you been?\" she asked. \xE2\x80\x9C" "Far away,\xE2\x80\x9D he said. "
    "The quick brown fox jumps over the lazy dog. 0123456789 "
    "Caf\xC3\xA9, na\xC3\xAFve, \xC3\xA0 la carte, A\xC3\xA7" "ores, Z\xC3\xBC" "rich, \xC5\x81\xC3\xB3" "d\xC5\xBA.";

} // namespace

FontPreviewView::FontPreviewView(SystemStyling &styling)
    : styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , font_name(styling.get_font_name())
    , font_size(styling.get_font_size())
{
}

FontPreviewView::~FontPreviewView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

bool FontPreviewView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *ui = cached_load_font(styling.get_font_name(), styling.get_font_size());
    if (!ui)
    {
        return true;
    }
    const auto &theme = styling.get_loaded_color_theme();
    const int line_height = TTF_FontHeight(ui) + 4;

    SDL_Rect all = {0, 0, static_cast<Uint16>(SCREEN_W), static_cast<Uint16>(SCREEN_H)};
    SDL_FillRect(dest_surface, &all, SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b));
    menu_draw::draw_title_bar(
        dest_surface, ui, theme, line_height,
        std::filesystem::path(font_name).filename().stem().string(),
        std::to_string(font_size)
    );

    if (TTF_Font *sample = cached_load_font(font_name, font_size))
    {
        const int top = line_height + 14;
        const int bottom = SCREEN_H - line_height - 10;
        const int step = TTF_FontHeight(sample) + 2;
        const size_t max_lines = static_cast<size_t>(std::max(1, (bottom - top) / step));
        const auto lines = menu_draw::wrap_lines(sample, SAMPLE, SCREEN_W - 40, max_lines);
        int y = top;
        for (const std::string &line : lines)
        {
            if (auto surface = menu_draw::shade(sample, line, theme.main_text, theme.background))
            {
                SDL_Rect at = {20, static_cast<Sint16>(y), 0, 0};
                SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
            }
            y += step;
        }
    }

    menu_draw::draw_hint_bar(
        dest_surface, ui, theme, line_height, {"A  Use", "Up/Down  Font", "Left/Right  Size", "B  Cancel"}
    );
    return true;
}

bool FontPreviewView::is_done()
{
    return _is_done;
}

bool FontPreviewView::is_modal()
{
    return true;
}

void FontPreviewView::on_keypress(SDLKey key)
{
    const uint32_t range = MAX_FONT_SIZE - MIN_FONT_SIZE + FONT_SIZE_STEP;
    switch (key)
    {
        case SW_BTN_UP:
            font_name = get_prev_font_name(font_name);
            break;
        case SW_BTN_DOWN:
            font_name = get_next_font_name(font_name);
            break;
        case SW_BTN_LEFT:
            font_size = (font_size - MIN_FONT_SIZE + range - FONT_SIZE_STEP) % range + MIN_FONT_SIZE;
            break;
        case SW_BTN_RIGHT:
            font_size = (font_size - MIN_FONT_SIZE + FONT_SIZE_STEP) % range + MIN_FONT_SIZE;
            break;
        case SW_BTN_A:
            styling.set_font_name(font_name);
            styling.set_font_size(font_size);
            _is_done = true;
            return;
        case SW_BTN_B:
        case SW_BTN_MENU:
            _is_done = true;
            return;
        default:
            return;
    }
    _needs_render = true;
}