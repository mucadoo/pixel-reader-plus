#include "./book_map_view.h"

#include "./menu_draw.h"
#include "doc_api/doc_reader.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <SDL/SDL.h>

#include <algorithm>
#include <cmath>

namespace
{

void fill(SDL_Surface *dest, int x, int y, int w, int h, SDL_Color color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    SDL_Rect rect = {static_cast<Sint16>(x), static_cast<Sint16>(y), static_cast<Uint16>(w), static_cast<Uint16>(h)};
    SDL_FillRect(dest, &rect, SDL_MapRGB(dest->format, color.r, color.g, color.b));
}

} // namespace

BookMapView::BookMapView(
    std::shared_ptr<DocReader> reader,
    DocAddr current,
    const std::vector<DocAddr> &bookmarks,
    const std::vector<DocAddr> &highlights,
    std::function<void(DocAddr)> on_go,
    SystemStyling &styling
)
    : reader(std::move(reader))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , on_go(std::move(on_go))
    , move_throttle(250, 45)
{
    const auto &toc = this->reader->get_table_of_contents();
    for (uint32_t i = 0; i < toc.size(); ++i)
    {
        const DocAddr address = this->reader->get_toc_item_address(i);
        chapter_addresses.push_back(address);
        chapter_marks.push_back(fraction_of(address));
    }
    for (DocAddr address : bookmarks)
    {
        bookmark_marks.push_back(fraction_of(address));
    }
    for (DocAddr address : highlights)
    {
        highlight_marks.push_back(fraction_of(address));
    }
    here = fraction_of(current);
    cursor = here;
    refresh_chapter_name();
}

BookMapView::~BookMapView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

double BookMapView::fraction_of(DocAddr address) const
{
    const auto extent = reader->get_progress_extent(address, false);
    if (extent.second == 0)
    {
        return 0.0;
    }
    return std::clamp(static_cast<double>(extent.first) / extent.second, 0.0, 1.0);
}

void BookMapView::refresh_chapter_name()
{
    chapter_name.clear();
    const auto &toc = reader->get_table_of_contents();
    if (toc.empty())
    {
        return;
    }
    // The last chapter that starts at or before the cursor
    int index = -1;
    for (size_t i = 0; i < chapter_marks.size(); ++i)
    {
        if (chapter_marks[i] <= cursor + 1e-9)
        {
            index = static_cast<int>(i);
        }
    }
    if (index >= 0)
    {
        chapter_name = toc[static_cast<size_t>(index)].display_name;
    }
}

void BookMapView::move_by(double amount)
{
    cursor = std::clamp(cursor + amount, 0.0, 1.0);
    cursor_chapter = -1;
    refresh_chapter_name();
    _needs_render = true;
}

void BookMapView::move_to_chapter(int direction)
{
    if (chapter_marks.empty())
    {
        return;
    }
    int target = -1;
    if (direction > 0)
    {
        for (size_t i = 0; i < chapter_marks.size(); ++i)
        {
            if (chapter_marks[i] > cursor + 1e-6)
            {
                target = static_cast<int>(i);
                break;
            }
        }
    }
    else
    {
        for (size_t i = chapter_marks.size(); i-- > 0;)
        {
            if (chapter_marks[i] < cursor - 1e-6)
            {
                target = static_cast<int>(i);
                break;
            }
        }
    }
    if (target < 0)
    {
        return;
    }
    cursor = chapter_marks[static_cast<size_t>(target)];
    cursor_chapter = target;
    refresh_chapter_name();
    _needs_render = true;
}

bool BookMapView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
    if (!font)
    {
        return true;
    }
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color quiet = menu_draw::quiet_text(theme);
    const int line_height = TTF_FontHeight(font) + 4;

    fill(dest_surface, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, theme.background);
    menu_draw::draw_title_bar(
        dest_surface, font, theme, line_height, "Book map", std::to_string(static_cast<int>(cursor * 100 + 0.5)) + "%"
    );

    const int side = 24;
    const int bar_w = SCREEN_WIDTH - side * 2;
    const int bar_h = 28;
    const int bar_y = SCREEN_HEIGHT / 2 - bar_h / 2 - line_height;
    auto x_of = [&](double fraction) { return side + static_cast<int>(std::lround(fraction * (bar_w - 1))); };

    // The bar, filled up to where the reader is
    fill(dest_surface, side - 1, bar_y - 1, bar_w + 2, bar_h + 2, quiet);
    fill(dest_surface, side, bar_y, bar_w, bar_h, theme.background);
    fill(dest_surface, side, bar_y, x_of(here) - side + 1, bar_h, theme.highlight_background);

    // Chapter starts cut the bar
    for (double mark : chapter_marks)
    {
        fill(dest_surface, x_of(mark), bar_y, 1, bar_h, mark <= here ? theme.background : quiet);
    }

    // Bookmarks above the bar, highlights below it
    for (double mark : bookmark_marks)
    {
        fill(dest_surface, x_of(mark) - 2, bar_y - 12, 5, 8, theme.main_text);
    }
    for (double mark : highlight_marks)
    {
        fill(dest_surface, x_of(mark) - 2, bar_y + bar_h + 5, 5, 8, theme.highlight_background);
    }

    // Where the reader is, and where the map points
    fill(dest_surface, x_of(here) - 1, bar_y - 6, 3, bar_h + 12, theme.main_text);
    const int cursor_x = x_of(cursor);
    fill(dest_surface, cursor_x - 2, bar_y - 18, 5, bar_h + 36, theme.highlight_background);
    fill(dest_surface, cursor_x - 1, bar_y - 18, 3, bar_h + 36, theme.highlight_text);

    // What is under the cursor
    const int text_y = bar_y + bar_h + 30;
    const std::string chapter_line = chapter_name.empty() ? std::string("Start of the book") : chapter_name;
    if (auto surface = menu_draw::shade(font, menu_draw::ellipsize(font, chapter_line, SCREEN_WIDTH - side * 2), theme.main_text, theme.background))
    {
        SDL_Rect at = {static_cast<Sint16>(SCREEN_WIDTH / 2 - surface->w / 2), static_cast<Sint16>(text_y), 0, 0};
        SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
    }
    const std::string legend = "Bright bar: read.  Squares above: bookmarks.  Below: highlights.";
    if (auto surface = menu_draw::shade(font, menu_draw::ellipsize(font, legend, SCREEN_WIDTH - side * 2), quiet, theme.background))
    {
        SDL_Rect at = {static_cast<Sint16>(SCREEN_WIDTH / 2 - surface->w / 2), static_cast<Sint16>(text_y + line_height + 4), 0, 0};
        SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
    }

    menu_draw::draw_hint_bar(
        dest_surface, font, theme, line_height, {"A  Go", "Left/Right  Move", "Up/Down  Chapter", "L/R  10%", "B  Close"}
    );
    return true;
}

bool BookMapView::is_done()
{
    return _is_done;
}

bool BookMapView::is_modal()
{
    return false;
}

void BookMapView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
            move_by(-0.01);
            break;
        case SW_BTN_RIGHT:
            move_by(0.01);
            break;
        case SW_BTN_L1:
            move_by(-0.10);
            break;
        case SW_BTN_R1:
            move_by(0.10);
            break;
        case SW_BTN_UP:
            move_to_chapter(-1);
            break;
        case SW_BTN_DOWN:
            move_to_chapter(1);
            break;
        case SW_BTN_A:
        {
            _is_done = true;
            if (on_go)
            {
                DocAddr address;
                if (cursor_chapter >= 0 && static_cast<size_t>(cursor_chapter) < chapter_addresses.size())
                {
                    address = chapter_addresses[static_cast<size_t>(cursor_chapter)];
                }
                else
                {
                    address = reader->get_address_at_percent(static_cast<uint32_t>(std::lround(cursor * 100)));
                }
                on_go(address);
            }
            return;
        }
        case SW_BTN_B:
        case SW_BTN_MENU:
            _is_done = true;
            return;
        default:
            break;
    }
}

void BookMapView::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    if ((key == SW_BTN_LEFT || key == SW_BTN_RIGHT || key == SW_BTN_L1 || key == SW_BTN_R1)
        && move_throttle(held_time_ms))
    {
        on_keypress(key);
    }
}
