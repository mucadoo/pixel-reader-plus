#include "./book_info_view.h"

#include "./menu_draw.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>

using namespace menu_draw;

BookInfoView::BookInfoView(
    std::string title,
    std::vector<std::pair<std::string, std::string>> facts,
    std::string description,
    SDL_Surface *source,
    SystemStyling &styling
)
    : title(std::move(title))
    , facts(std::move(facts))
    , description(std::move(description))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
    if (source)
    {
        cover = SDL_ConvertSurface(source, source->format, SDL_SWSURFACE);
    }
}

BookInfoView::~BookInfoView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
    if (cover)
    {
        SDL_FreeSurface(cover);
    }
}

bool BookInfoView::render(SDL_Surface *dest, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *font = styling.get_loaded_font();
    const ColorTheme &theme = styling.get_loaded_color_theme();
    line_h = detect_line_height(font) + 4;
    const int width = static_cast<int>(SCREEN_WIDTH);
    const int height = static_cast<int>(SCREEN_HEIGHT);
    const int pad = 12;
    const int top = line_h + 6;
    view_h = height - top - line_h;

    SDL_Rect all = {0, 0, static_cast<Uint16>(width), static_cast<Uint16>(height)};
    SDL_FillRect(dest, &all, SDL_MapRGB(dest->format, theme.background.r, theme.background.g, theme.background.b));

    // Lay the text out: beside the cover while the cover lasts, then across the page.
    const int cover_w = cover ? cover->w : 0;
    const int cover_h = cover ? cover->h : 0;
    const int narrow_x = pad + (cover ? cover_w + pad : 0);
    const int narrow_w = width - narrow_x - pad;
    const int wide_w = width - pad * 2;
    std::vector<Line> lines;
    int y = 0;
    auto add = [&](const std::string &text, bool quiet) {
        const bool beside = cover && y < cover_h;
        const int w = beside ? narrow_w : wide_w;
        for (const std::string &piece : wrap_lines(font, text, w, 400))
        {
            const bool here = cover && y < cover_h;
            Line line;
            line.text = piece;
            line.x = here ? narrow_x : pad;
            line.y = y;
            line.quiet = quiet;
            lines.push_back(line);
            y += line_h;
        }
    };
    for (const auto &fact : facts)
    {
        if (!fact.second.empty())
        {
            add(fact.first + ": " + fact.second, false);
        }
    }
    if (!description.empty())
    {
        y += line_h / 2;
        add("Description", true);
        add(description, false);
    }
    y = std::max(y, cover_h);
    content_h = y;
    scroll_px = std::clamp(scroll_px, 0, std::max(0, content_h - view_h));

    // The content is drawn into the band between the title and the hints
    SDL_Rect clip = {0, static_cast<Sint16>(top), static_cast<Uint16>(width), static_cast<Uint16>(view_h)};
    SDL_SetClipRect(dest, &clip);
    if (cover)
    {
        SDL_Rect where = {static_cast<Sint16>(pad), static_cast<Sint16>(top - scroll_px), 0, 0};
        SDL_BlitSurface(cover, nullptr, dest, &where);
    }
    for (const Line &line : lines)
    {
        const int line_y = top + line.y - scroll_px;
        if (line_y + line_h < top || line_y > top + view_h || line.text.empty())
        {
            continue;
        }
        auto surface = shade(font, line.text, line.quiet ? quiet_text(theme) : theme.main_text, theme.background);
        if (surface)
        {
            blit_crop(surface.get(), dest, line.x, line_y, width - line.x - pad);
        }
    }
    SDL_SetClipRect(dest, nullptr);

    const std::string counter = content_h > view_h
        ? std::to_string(std::min(100, (scroll_px + view_h) * 100 / std::max(1, content_h))) + "%"
        : std::string();
    draw_title_bar(dest, font, theme, line_h, title, counter);
    draw_hint_bar(dest, font, theme, line_h, {"Up/Down  Scroll", "B  Close"});
    return true;
}

bool BookInfoView::is_done()
{
    return _is_done;
}

bool BookInfoView::is_modal()
{
    return false;
}

void BookInfoView::on_keypress(SDLKey key)
{
    const int step = std::max(1, line_h);
    if (key == SW_BTN_UP || key == SW_BTN_DOWN || key == SW_BTN_LEFT || key == SW_BTN_RIGHT
        || key == SW_BTN_L1 || key == SW_BTN_R1)
    {
        int delta = key == SW_BTN_UP ? -step : key == SW_BTN_DOWN ? step : key == SW_BTN_LEFT || key == SW_BTN_L1 ? -view_h : view_h;
        const int next = std::clamp(scroll_px + delta, 0, std::max(0, content_h - view_h));
        if (next != scroll_px)
        {
            scroll_px = next;
            _needs_render = true;
        }
        return;
    }
    _is_done = true;
}

void BookInfoView::on_keyheld(SDLKey key, uint32_t)
{
    if (key == SW_BTN_UP || key == SW_BTN_DOWN)
    {
        on_keypress(key);
    }
}
