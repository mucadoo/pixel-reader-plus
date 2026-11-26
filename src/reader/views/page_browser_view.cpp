#include "./page_browser_view.h"

#include "./menu_draw.h"
#include "doc_api/doc_reader.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <SDL/SDL.h>

#include <algorithm>
#include <cmath>

namespace
{

constexpr int TEXT_TILES = 50;      // a tile for every 2% of a book
const int SCREEN_W = static_cast<int>(::SCREEN_WIDTH);
const int SCREEN_H = static_cast<int>(::SCREEN_HEIGHT);

void fill(SDL_Surface *dest, int x, int y, int w, int h, SDL_Color color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    SDL_Rect rect = {static_cast<Sint16>(x), static_cast<Sint16>(y), static_cast<Uint16>(w), static_cast<Uint16>(h)};
    SDL_FillRect(dest, &rect, SDL_MapRGB(dest->format, color.r, color.g, color.b));
}

// The words of a token, if it has any.
bool token_text(const DocToken &token, std::string &out)
{
    switch (token.type)
    {
        case TokenType::Text:
            out = static_cast<const TextDocToken &>(token).text;
            return true;
        case TokenType::Header:
            out = static_cast<const HeaderDocToken &>(token).text;
            return true;
        case TokenType::ListItem:
            out = static_cast<const ListItemDocToken &>(token).text;
            return true;
        default:
            return false;
    }
}

} // namespace

PageBrowserView::PageBrowserView(
    std::shared_ptr<DocReader> reader,
    DocAddr current,
    std::function<void(DocAddr)> on_go,
    SystemStyling &styling
)
    : reader(std::move(reader))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , on_go(std::move(on_go))
{
    tile_count = TEXT_TILES;
    here_tile = tile_of(current);
    cursor = here_tile;
    first_row = std::max(0, cursor / cols - rows / 2);
}

PageBrowserView::~PageBrowserView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

DocAddr PageBrowserView::address_of(int tile) const
{
    return reader->get_address_at_percent(static_cast<uint32_t>(tile * 100 / TEXT_TILES));
}

int PageBrowserView::tile_of(DocAddr address) const
{
    const auto extent = reader->get_progress_extent(address, false);
    if (extent.second == 0)
    {
        return 0;
    }
    const double fraction = static_cast<double>(extent.first) / extent.second;
    return std::clamp(static_cast<int>(fraction * TEXT_TILES), 0, tile_count - 1);
}

int PageBrowserView::visible_first() const
{
    return first_row * cols;
}

int PageBrowserView::visible_last() const
{
    return std::min(tile_count, (first_row + rows) * cols) - 1;
}

void PageBrowserView::load_snippet(int tile)
{
    std::string text;
    auto iter = reader->get_iter(address_of(tile));
    for (int guard = 0; iter && guard < 40 && text.size() < 220; ++guard)
    {
        const DocToken *token = iter->read(1);
        if (!token)
        {
            break;
        }
        std::string part;
        if (token_text(*token, part))
        {
            for (char &c : part)
            {
                if (c == '\n' || c == '\t')
                {
                    c = ' ';
                }
            }
            while (!part.empty() && part.front() == ' ')
            {
                part.erase(0, 1);
            }
            if (!part.empty())
            {
                text += (text.empty() ? "" : " ") + part;
            }
        }
    }
    std::vector<std::string> lines;
    lines.push_back(text);
    snippets[tile] = lines;
}

// Reads the content of one visible tile that has none yet. True when it did.
bool PageBrowserView::load_one_pending()
{
    for (int tile = visible_first(); tile <= visible_last(); ++tile)
    {
        if (!snippets.count(tile))
        {
            load_snippet(tile);
            return true;
        }
    }
    return false;
}

bool PageBrowserView::tick(uint32_t)
{
    return load_one_pending();
}

void PageBrowserView::move_cursor(int cols_delta, int rows_delta)
{
    int next = cursor + cols_delta + rows_delta * cols;
    next = std::clamp(next, 0, tile_count - 1);
    cursor = next;
    const int row = cursor / cols;
    if (row < first_row)
    {
        first_row = row;
    }
    else if (row >= first_row + rows)
    {
        first_row = row - rows + 1;
    }
    _needs_render = true;
}

bool PageBrowserView::render(SDL_Surface *dest_surface, bool force_render)
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
    const uint32_t small_size = static_cast<uint32_t>(std::clamp(static_cast<int>(styling.get_font_size()) * 2 / 3, 12, 20));
    TTF_Font *small = cached_load_font(styling.get_font_name(), small_size);
    if (!small)
    {
        small = font;
    }
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color quiet = menu_draw::quiet_text(theme);
    const int line_height = TTF_FontHeight(font) + 4;
    const int small_height = TTF_FontHeight(small);

    fill(dest_surface, 0, 0, SCREEN_W, SCREEN_H, theme.background);
    const int pages = (tile_count + cols * rows - 1) / (cols * rows);
    const int page = first_row / rows + 1;
    menu_draw::draw_title_bar(
        dest_surface, font, theme, line_height,
        "Skim", std::to_string(page) + " / " + std::to_string(pages)
    );

    const int top = line_height + 10;
    const int bottom = SCREEN_H - line_height - 10;
    const int gap = 6;
    const int tile_w = (SCREEN_W - gap * (cols + 1)) / cols;
    const int tile_h = (bottom - top - gap * (rows + 1)) / rows;

    for (int tile = visible_first(); tile <= visible_last(); ++tile)
    {
        const int row = tile / cols - first_row;
        const int col = tile % cols;
        const int x = gap + col * (tile_w + gap);
        const int y = top + gap + row * (tile_h + gap);
        const bool selected = tile == cursor;
        const SDL_Color back = selected ? theme.highlight_background : theme.background;
        const SDL_Color ink = selected ? theme.highlight_text : theme.main_text;
        const SDL_Color faint = selected ? theme.highlight_text : quiet;
        fill(dest_surface, x, y, tile_w, tile_h, selected ? theme.highlight_text : quiet);
        fill(dest_surface, x + 1, y + 1, tile_w - 2, tile_h - 2, back);

        std::string label = std::to_string(tile * 2) + "%";
        {
            const auto position = reader->get_toc_position(address_of(tile));
            const auto &toc = reader->get_table_of_contents();
            if (!toc.empty() && position.toc_index < toc.size())
            {
                label += "  " + toc[position.toc_index].display_name;
            }
        }
        if (tile == here_tile)
        {
            label = "> " + label;
        }
        if (auto surface = menu_draw::shade(small, menu_draw::ellipsize(small, label, tile_w - 8), ink, back))
        {
            SDL_Rect at = {static_cast<Sint16>(x + 4), static_cast<Sint16>(y + 3), 0, 0};
            SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
        }

        const int body_y = y + small_height + 6;
        {
            const auto found = snippets.find(tile);
            if (found != snippets.end() && !found->second.empty())
            {
                const size_t max_lines = static_cast<size_t>(std::max(1, (tile_h - small_height - 12) / small_height));
                const auto lines = menu_draw::wrap_lines(small, found->second[0], tile_w - 8, max_lines);
                int line_y = body_y;
                for (const std::string &line : lines)
                {
                    if (auto surface = menu_draw::shade(small, line, faint, back))
                    {
                        SDL_Rect at = {static_cast<Sint16>(x + 4), static_cast<Sint16>(line_y), 0, 0};
                        SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
                    }
                    line_y += small_height;
                }
            }
        }
    }

    menu_draw::draw_hint_bar(
        dest_surface, font, theme, line_height, {"A  Go", "D-pad  Move", "L/R  Screen", "B  Close"}
    );
    return true;
}

bool PageBrowserView::is_done()
{
    return _is_done;
}

bool PageBrowserView::is_modal()
{
    return false;
}

void PageBrowserView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
            move_cursor(-1, 0);
            break;
        case SW_BTN_RIGHT:
            move_cursor(1, 0);
            break;
        case SW_BTN_UP:
            move_cursor(0, -1);
            break;
        case SW_BTN_DOWN:
            move_cursor(0, 1);
            break;
        case SW_BTN_L1:
        case SW_BTN_L2:
            move_cursor(0, -rows);
            break;
        case SW_BTN_R1:
        case SW_BTN_R2:
            move_cursor(0, rows);
            break;
        case SW_BTN_A:
            _is_done = true;
            mark_completed();
            if (on_go)
            {
                on_go(address_of(cursor));
            }
            return;
        case SW_BTN_B:
        case SW_BTN_MENU:
            _is_done = true;
            return;
        default:
            break;
    }
}
