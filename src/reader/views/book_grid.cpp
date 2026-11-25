#include "./book_grid.h"

#include "./menu_draw.h"
#include "reader/book_covers.h"
#include "reader/config.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <cstdio>
#include <functional>

using namespace menu_draw;

namespace
{

constexpr uint32_t CAPTION_FONT_SIZE = 17;
constexpr int CELL_PAD = 6;
constexpr int MARGIN_X = 10;
constexpr int MIN_COVER_H = 48;

SDL_Color blend(SDL_Color a, SDL_Color b, int b_percent)
{
    auto mix = [b_percent](int x, int y) {
        return static_cast<Uint8>((x * (100 - b_percent) + y * b_percent) / 100);
    };
    return SDL_Color { mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 0 };
}

uint32_t map_color(SDL_Surface *dest, SDL_Color c)
{
    return SDL_MapRGB(dest->format, c.r, c.g, c.b);
}

void fill(SDL_Surface *dest, int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    SDL_Rect rect = {
        static_cast<Sint16>(x),
        static_cast<Sint16>(y),
        static_cast<Uint16>(w),
        static_cast<Uint16>(h)
    };
    SDL_FillRect(dest, &rect, color);
}

void outline(SDL_Surface *dest, int x, int y, int w, int h, uint32_t color)
{
    fill(dest, x, y, w, 1, color);
    fill(dest, x, y + h - 1, w, 1, color);
    fill(dest, x, y, 1, h, color);
    fill(dest, x + w - 1, y, 1, h, color);
}

bool is_move_key(SDLKey key)
{
    return key == SW_BTN_UP || key == SW_BTN_DOWN || key == SW_BTN_LEFT || key == SW_BTN_RIGHT;
}

} // namespace

BookGrid::BookGrid(SystemStyling &styling)
    : styling(styling),
      styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          needs_render = true;
          marquee.reset();
      })),
      move_throttle(250, 100)
{
}

BookGrid::~BookGrid()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void BookGrid::set_covers(BookCovers *new_covers)
{
    covers = new_covers;
    needs_render = true;
}

void BookGrid::set_items(std::vector<Item> new_items)
{
    items = std::move(new_items);
    cursor_pos = 0;
    first_row = 0;
    marquee.reset();
    needs_render = true;
}

void BookGrid::set_title(std::string new_title)
{
    title = std::move(new_title);
    needs_render = true;
}

void BookGrid::set_hints(std::vector<std::string> new_hints)
{
    hints = std::move(new_hints);
    needs_render = true;
}

void BookGrid::set_empty_message(std::string message)
{
    empty_message = std::move(message);
    needs_render = true;
}

void BookGrid::set_cursor(uint32_t index)
{
    if (index >= items.size())
    {
        index = 0;
    }
    cursor_pos = index;
    marquee.reset();
    ensure_visible();
    needs_render = true;
}

uint32_t BookGrid::cursor() const
{
    return cursor_pos;
}

void BookGrid::set_on_focus(std::function<void(uint32_t)> callback)
{
    on_focus = std::move(callback);
}

void BookGrid::set_on_open(std::function<void(uint32_t)> callback)
{
    on_open = std::move(callback);
}

void BookGrid::invalidate()
{
    needs_render = true;
}

TTF_Font *BookGrid::caption_font() const
{
    TTF_Font *font = cached_load_font(styling.get_font_name(), CAPTION_FONT_SIZE, FontLoadErrorOpt::NoThrow);
    return font ? font : styling.get_loaded_font();
}

BookGrid::Layout BookGrid::compute_layout() const
{
    Layout l;
    l.line_height = detect_line_height(styling.get_font_name(), styling.get_font_size()) + 4;

    TTF_Font *small = caption_font();
    l.caption_height = (small ? detect_line_height(small) : 18) + 6;

    const int width = static_cast<int>(SCREEN_WIDTH);
    const int height = static_cast<int>(SCREEN_HEIGHT);
    const int area_h = std::max(1, height - l.line_height * 2);

    l.rows = std::clamp(area_h / 200, 1, 4);
    l.cell_h = area_h / l.rows;
    l.cover_h = std::max(MIN_COVER_H, l.cell_h - l.caption_height - CELL_PAD * 2);
    l.cover_w = l.cover_h * 2 / 3;
    l.cols = std::max(1, (width - MARGIN_X * 2) / (l.cover_w + 12));
    l.cell_w = (width - MARGIN_X * 2) / l.cols;
    if (l.cover_w > l.cell_w - 8)
    {
        l.cover_w = std::max(24, l.cell_w - 8);
        l.cover_h = l.cover_w * 3 / 2;
    }
    l.origin_x = MARGIN_X;
    l.origin_y = l.line_height + std::max(0, (area_h - l.cell_h * l.rows) / 2);
    return l;
}

void BookGrid::ensure_visible()
{
    Layout l = compute_layout();
    const uint32_t cols = static_cast<uint32_t>(l.cols);
    const uint32_t rows = static_cast<uint32_t>(l.rows);
    const uint32_t row = cursor_pos / cols;
    if (row < first_row)
    {
        first_row = row;
    }
    else if (row >= first_row + rows)
    {
        first_row = row - rows + 1;
    }

    const uint32_t total_rows = items.empty() ? 1 : static_cast<uint32_t>((items.size() + cols - 1) / cols);
    const uint32_t max_first = total_rows > rows ? total_rows - rows : 0;
    first_row = std::min(first_row, max_first);
}

void BookGrid::move_to(uint32_t index)
{
    if (items.empty())
    {
        return;
    }
    index = std::min<uint32_t>(index, static_cast<uint32_t>(items.size()) - 1);
    if (index == cursor_pos)
    {
        return;
    }
    cursor_pos = index;
    marquee.reset();
    ensure_visible();
    needs_render = true;
    if (on_focus)
    {
        on_focus(cursor_pos);
    }
}

void BookGrid::on_keypress(SDLKey key)
{
    if (items.empty())
    {
        return;
    }

    const Layout l = compute_layout();
    const int count = static_cast<int>(items.size());
    const int cols = l.cols;
    const int page = cols * l.rows;
    const int current = static_cast<int>(cursor_pos);
    const auto shoulders = get_shoulder_keymap_lr(styling.get_shoulder_keymap());

    if (key == SW_BTN_LEFT)
    {
        move_to(static_cast<uint32_t>(std::max(0, current - 1)));
    }
    else if (key == SW_BTN_RIGHT)
    {
        move_to(static_cast<uint32_t>(std::min(count - 1, current + 1)));
    }
    else if (key == SW_BTN_UP)
    {
        if (current >= cols)
        {
            move_to(static_cast<uint32_t>(current - cols));
        }
    }
    else if (key == SW_BTN_DOWN)
    {
        if (current + cols < count)
        {
            move_to(static_cast<uint32_t>(current + cols));
        }
        else if (current / cols < (count - 1) / cols)
        {
            move_to(static_cast<uint32_t>(count - 1)); // the last row is short
        }
    }
    else if (key == shoulders.first)
    {
        move_to(static_cast<uint32_t>(std::max(0, current - page)));
    }
    else if (key == shoulders.second)
    {
        move_to(static_cast<uint32_t>(std::min(count - 1, current + page)));
    }
    else if (key == SW_BTN_A)
    {
        if (on_open)
        {
            on_open(cursor_pos);
        }
    }
}

void BookGrid::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    const auto shoulders = get_shoulder_keymap_lr(styling.get_shoulder_keymap());
    if (is_move_key(key) || key == shoulders.first || key == shoulders.second)
    {
        if (move_throttle(held_time_ms))
        {
            on_keypress(key);
        }
    }
}

bool BookGrid::tick(uint32_t elapsed_ms)
{
    bool redraw = false;

    if (covers)
    {
        const Layout l = compute_layout();
        covers->set_box(l.cover_w, l.cover_h);

        // The books on screen, nearest the cursor first.
        std::vector<uint32_t> visible;
        const uint32_t first = first_row * static_cast<uint32_t>(l.cols);
        const uint32_t last = std::min<uint32_t>(
            static_cast<uint32_t>(items.size()),
            first + static_cast<uint32_t>(l.cols * l.rows)
        );
        for (uint32_t i = first; i < last; ++i)
        {
            if (items[i].kind == Item::Kind::Book)
            {
                visible.push_back(i);
            }
        }
        std::sort(visible.begin(), visible.end(), [this](uint32_t a, uint32_t b) {
            auto distance = [this](uint32_t i) {
                return i > cursor_pos ? i - cursor_pos : cursor_pos - i;
            };
            return distance(a) < distance(b);
        });
        std::vector<std::filesystem::path> wanted;
        wanted.reserve(visible.size());
        for (uint32_t i : visible)
        {
            wanted.push_back(items[i].path);
        }
        covers->want(wanted);

        if (covers->poll())
        {
            redraw = true;
        }
    }

    if (marquee.tick(elapsed_ms))
    {
        redraw = true;
    }

    if (redraw)
    {
        needs_render = true;
    }
    return redraw;
}

void BookGrid::draw_placeholder(SDL_Surface *dest, const Item &item, int x, int y, int w, int h, bool selected)
{
    const ColorTheme &theme = styling.get_loaded_color_theme();
    TTF_Font *font = caption_font();
    const int font_h = font ? detect_line_height(font) : 16;
    const SDL_Color text_color = {240, 236, 226, 0};

    if (item.kind == Item::Kind::Book)
    {
        static const SDL_Color palette[] = {
            {110, 70, 62, 0}, {64, 92, 118, 0}, {78, 106, 74, 0},
            {116, 98, 58, 0}, {92, 72, 114, 0}, {58, 98, 98, 0},
        };
        const SDL_Color body = palette[std::hash<std::string>()(item.label) % (sizeof(palette) / sizeof(palette[0]))];
        fill(dest, x, y, w, h, map_color(dest, body));
        fill(dest, x, y, 5, h, map_color(dest, blend(body, SDL_Color {0, 0, 0, 0}, 35))); // spine

        const int text_x = x + 12;
        const int text_w = w - 18;
        const size_t max_lines = static_cast<size_t>(std::max(1, (h - font_h - 24) / std::max(1, font_h)));
        std::vector<std::string> lines = wrap_lines(font, item.label, text_w, max_lines);
        int line_y = y + 10;
        for (const auto &line : lines)
        {
            auto surface = shade(font, line, text_color, body);
            blit_crop(surface.get(), dest, text_x, line_y, text_w);
            line_y += font_h;
        }
        if (!item.tag.empty())
        {
            auto tag = shade(font, item.tag, blend(body, text_color, 70), body);
            blit_crop(tag.get(), dest, text_x, y + h - font_h - 8, text_w);
        }
        return;
    }

    // Folder tiles and the way up are drawn in the theme's own tones.
    const SDL_Color body = blend(theme.background, theme.main_text, selected ? 38 : 26);
    const SDL_Color edge = blend(theme.background, theme.main_text, 55);
    if (item.kind == Item::Kind::Folder)
    {
        const int tab_h = std::max(8, h / 14);
        fill(dest, x, y, w * 2 / 5, tab_h + 2, map_color(dest, edge));
        fill(dest, x, y + tab_h, w, h - tab_h, map_color(dest, body));
        outline(dest, x, y + tab_h, w, h - tab_h, map_color(dest, edge));
    }
    else
    {
        fill(dest, x, y, w, h, map_color(dest, body));
        outline(dest, x, y, w, h, map_color(dest, edge));
    }

    const std::string word = item.kind == Item::Kind::Up ? "Up" : "Folder";
    auto surface = shade(font, word, theme.main_text, body);
    if (surface)
    {
        blit_crop(surface.get(), dest, x + std::max(0, (w - surface->w) / 2), y + (h - surface->h) / 2, w);
    }
}

void BookGrid::draw_cell(SDL_Surface *dest, const Layout &l, uint32_t index, int cell_x, int cell_y)
{
    const ColorTheme &theme = styling.get_loaded_color_theme();
    const Item &item = items[index];
    const bool selected = index == cursor_pos;
    TTF_Font *font = caption_font();

    if (selected)
    {
        fill(dest, cell_x + 2, cell_y + 2, l.cell_w - 4, l.cell_h - 4, map_color(dest, theme.highlight_background));
    }

    const int cover_x = cell_x + (l.cell_w - l.cover_w) / 2;
    const int cover_y = cell_y + CELL_PAD;

    int art_x = cover_x;
    int art_y = cover_y;
    int art_w = l.cover_w;
    int art_h = l.cover_h;
    SDL_Surface *picture = (covers && item.kind == Item::Kind::Book) ? covers->get(item.path) : nullptr;
    if (picture)
    {
        const int pic_x = cover_x + (l.cover_w - picture->w) / 2;
        const int pic_y = cover_y + (l.cover_h - picture->h) / 2;
        art_x = pic_x;
        art_y = pic_y;
        art_w = picture->w;
        art_h = picture->h;
        SDL_Rect where = {static_cast<Sint16>(pic_x), static_cast<Sint16>(pic_y), 0, 0};
        SDL_BlitSurface(picture, nullptr, dest, &where);
        outline(dest, pic_x, pic_y, picture->w, picture->h, map_color(dest, quiet_text(theme)));
    }
    else
    {
        draw_placeholder(dest, item, cover_x, cover_y, l.cover_w, l.cover_h, selected);
    }

    // How far into the book the reader is: a bar along the bottom of the cover.
    if (item.kind == Item::Kind::Book && item.progress >= 0 && art_w > 8 && art_h > 12)
    {
        const int bar_h = 5;
        const int bar_x = art_x + 2;
        const int bar_y = art_y + art_h - bar_h - 2;
        const int bar_w = art_w - 4;
        fill(dest, bar_x - 1, bar_y - 1, bar_w + 2, bar_h + 2, map_color(dest, theme.main_text));
        fill(dest, bar_x, bar_y, bar_w, bar_h, map_color(dest, theme.background));
        const int done_w = std::min(bar_w, bar_w * std::min(item.progress, 100) / 100);
        if (done_w > 0)
        {
            fill(dest, bar_x, bar_y, done_w, bar_h, map_color(dest, theme.main_text));
        }
    }

    // The name under the cover. The chosen one slides when it is too long.
    const SDL_Color caption_fg = selected ? theme.highlight_text : theme.main_text;
    const SDL_Color caption_bg = selected ? theme.highlight_background : theme.background;
    const int caption_x = cell_x + 5;
    const int caption_w = l.cell_w - 10;
    const int caption_y = cover_y + l.cover_h + 3;
    if (selected)
    {
        auto surface = shade(font, item.label, caption_fg, caption_bg);
        marquee.set_overflow(surface ? surface->w - caption_w : 0);
        blit_crop(surface.get(), dest, caption_x, caption_y, caption_w, marquee.offset());
    }
    else
    {
        auto surface = shade(font, ellipsize(font, item.label, caption_w), caption_fg, caption_bg);
        blit_crop(surface.get(), dest, caption_x, caption_y, caption_w);
    }
}

bool BookGrid::render(SDL_Surface *dest, bool force_render)
{
    if (!needs_render && !force_render)
    {
        return false;
    }
    needs_render = false;

    ensure_visible();
    const Layout l = compute_layout();
    const ColorTheme &theme = styling.get_loaded_color_theme();
    TTF_Font *font = styling.get_loaded_font();

    fill(dest, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, map_color(dest, theme.background));

    char counter[32] = "";
    if (!items.empty())
    {
        std::snprintf(counter, sizeof(counter), "%u / %u", cursor_pos + 1, static_cast<unsigned>(items.size()));
    }
    draw_title_bar(dest, font, theme, l.line_height, title, counter);

    const uint32_t cols = static_cast<uint32_t>(l.cols);
    for (uint32_t r = 0; r < static_cast<uint32_t>(l.rows); ++r)
    {
        for (uint32_t c = 0; c < cols; ++c)
        {
            const uint32_t index = (first_row + r) * cols + c;
            if (index >= items.size())
            {
                break;
            }
            draw_cell(
                dest,
                l,
                index,
                l.origin_x + static_cast<int>(c) * l.cell_w,
                l.origin_y + static_cast<int>(r) * l.cell_h
            );
        }
    }

    if (!empty_message.empty())
    {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= empty_message.size())
        {
            size_t end = empty_message.find('\n', start);
            if (end == std::string::npos)
            {
                end = empty_message.size();
            }
            lines.push_back(empty_message.substr(start, end - start));
            start = end + 1;
        }
        const SDL_Color quiet = quiet_text(theme);
        const int area_h = static_cast<int>(SCREEN_HEIGHT) - l.line_height * 2;
        const int block_h = static_cast<int>(lines.size()) * l.line_height;
        const int shown_rows = static_cast<int>(std::min<size_t>(l.rows, (items.size() + cols - 1) / cols));
        int top = l.line_height + (area_h - block_h) / 2;
        top = std::max(top, l.origin_y + shown_rows * l.cell_h + l.line_height / 2);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto line = shade(font, lines[i], i == 0 ? theme.main_text : quiet, theme.background);
            if (line)
            {
                blit_crop(
                    line.get(),
                    dest,
                    std::max(14, (static_cast<int>(SCREEN_WIDTH) - line->w) / 2),
                    top + static_cast<int>(i) * l.line_height,
                    static_cast<int>(SCREEN_WIDTH) - 28
                );
            }
        }
    }

    draw_hint_bar(dest, font, theme, l.line_height, hints);
    return true;
}
