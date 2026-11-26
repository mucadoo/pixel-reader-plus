#include "./goto_view.h"

#include "./menu_draw.h"
#include "reader/draw_modal_border.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>

GotoView::GotoView(
    std::string heading,
    std::string suffix,
    uint32_t lo,
    uint32_t hi,
    uint32_t start,
    std::function<void(uint32_t)> on_choose,
    SystemStyling &styling
)
    : heading(std::move(heading))
    , suffix(std::move(suffix))
    , lo(lo)
    , hi(std::max(lo, hi))
    , percent(std::clamp(start, lo, std::max(lo, hi)))
    , on_choose(std::move(on_choose))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
}

GotoView::~GotoView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

bool GotoView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }

    TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color quiet = menu_draw::quiet_text(theme);

    auto title = menu_draw::shade(font, heading, quiet, theme.background);
    auto value = menu_draw::shade(font, std::to_string(percent) + suffix, theme.main_text, theme.background);
    auto hint = menu_draw::shade(font, "Left/Right 1   Up/Down 10", quiet, theme.background);
    auto hint2 = menu_draw::shade(font, "A  Go    B  Cancel", quiet, theme.background);
    if (!title || !value || !hint || !hint2)
    {
        return false;
    }

    const int gap = 6;
    const int w = std::max({title->w, value->w, hint->w, hint2->w});
    const int h = title->h + value->h + hint->h + hint2->h + gap * 3;
    draw_modal_border(w, h, theme, dest_surface);

    int y = SCREEN_HEIGHT / 2 - h / 2;
    for (SDL_Surface *line : {title.get(), value.get(), hint.get(), hint2.get()})
    {
        SDL_Rect rect = {
            static_cast<Sint16>(SCREEN_WIDTH / 2 - line->w / 2),
            static_cast<Sint16>(y),
            0,
            0
        };
        SDL_BlitSurface(line, NULL, dest_surface, &rect);
        y += line->h + gap;
    }

    _needs_render = false;
    return true;
}

bool GotoView::is_done()
{
    return _is_done;
}

bool GotoView::is_modal()
{
    return true;
}

void GotoView::on_keypress(SDLKey key)
{
    const uint32_t before = percent;
    switch (key)
    {
        case SW_BTN_LEFT:
            percent = percent > lo ? percent - 1 : lo;
            break;
        case SW_BTN_RIGHT:
            percent = std::min<uint32_t>(percent + 1, hi);
            break;
        case SW_BTN_DOWN:
            percent = percent > lo + 10 ? percent - 10 : lo;
            break;
        case SW_BTN_UP:
            percent = std::min<uint32_t>(percent + 10, hi);
            break;
        case SW_BTN_A:
            _is_done = true;
            mark_completed();
            if (on_choose)
            {
                on_choose(percent);
            }
            return;
        case SW_BTN_B:
            _is_done = true;
            return;
        default:
            break;
    }
    if (percent != before)
    {
        _needs_render = true;
    }
}