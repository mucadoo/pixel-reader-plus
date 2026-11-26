#include "./text_input_view.h"

#include "./menu_draw.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/utf8.h"

#include <SDL/SDL.h>

#include <algorithm>

namespace
{

constexpr int COLUMNS = 10;
constexpr int ROWS = 4;
constexpr int PAGES = 5;

// Ten keys to a row, four rows to a page. Select moves to the next page.
const char *const PAGE_KEYS[PAGES][ROWS] = {
    {"qwertyuiop", "asdfghjkl\x27", "zxcvbnm,.-", "0123456789"},
    {"QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;:_", "0123456789"},
    {"\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\xC5\x93\xC3\x9F",
     "\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB1\xC3\xBD",
     "\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xB8\xC3\xB9\xC3\xBA\xC3\xBB\xC3\xBC",
     "\xC3\xBF\xC2\xBF\xC2\xA1\xC2\xAB\xC2\xBB\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x9D\xE2\x80\x93\xE2\x80\xA6"},
    {"\xC3\x80\xC3\x81\xC3\x82\xC3\x83\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC5\x92\xC3\x9E",
     "\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x91\xC3\x9D",
     "\xC3\x92\xC3\x93\xC3\x94\xC3\x95\xC3\x96\xC3\x98\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C",
     "\xC3\x90\xC3\xB0\xC3\xBE\xC2\xA7\xC2\xB0\xC2\xA9\xC2\xAE\xC2\xB5\xC2\xA3\xE2\x82\xAC"},
    {"!?()[]{}<>", "@#%&*+=/\\|", "~^`$;:\x27\"_-", "0123456789"},
};

const char *const PAGE_NAMES[PAGES] = {"abc", "ABC", "\xC3\xA0\xC3\xA9\xC3\xAE", "\xC3\x80\xC3\x89\xC3\x8E", "#+="};

// The nth character of a row as its own string.
std::string key_at(int page, int row, int col)
{
    const char *p = PAGE_KEYS[page][row];
    for (int i = 0; i < col && *p; ++i)
    {
        p = utf8_step(p);
    }
    if (!*p)
    {
        return std::string();
    }
    return std::string(p, utf8_step(p) - p);
}

} // namespace

TextInputView::TextInputView(
    std::string heading,
    std::string initial,
    std::function<void(const std::string &)> on_done,
    SystemStyling &styling,
    size_t max_length
)
    : heading(std::move(heading))
    , text(std::move(initial))
    , max_length(max_length)
    , on_done(std::move(on_done))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
}

TextInputView::~TextInputView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void TextInputView::type_key()
{
    const std::string key = key_at(page, cursor_row, cursor_col);
    if (!key.empty() && text.size() + key.size() <= max_length)
    {
        text += key;
        _needs_render = true;
    }
}

void TextInputView::erase_last()
{
    if (text.empty())
    {
        return;
    }
    size_t cut = text.size() - 1;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
    {
        --cut;
    }
    text.erase(cut);
    _needs_render = true;
}

bool TextInputView::render(SDL_Surface *dest_surface, bool force_render)
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

    SDL_FillRect(dest_surface, nullptr, SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b));

    menu_draw::draw_title_bar(dest_surface, font, theme, line_height, heading, PAGE_NAMES[page]);

    // The text typed so far, with its end kept in view
    const int side = 16;
    const int field_y = line_height + 12;
    const int field_w = SCREEN_WIDTH - side * 2;
    std::string shown = text + "_";
    while (shown.size() > 1 && menu_draw::text_width(font, shown) > field_w - 12)
    {
        shown.erase(0, utf8_step(shown.c_str()) - shown.c_str());
    }
    SDL_Rect box = {static_cast<Sint16>(side), static_cast<Sint16>(field_y), static_cast<Uint16>(field_w), static_cast<Uint16>(line_height + 8)};
    SDL_FillRect(dest_surface, &box, SDL_MapRGB(dest_surface->format, quiet.r, quiet.g, quiet.b));
    SDL_Rect inner = {static_cast<Sint16>(box.x + 1), static_cast<Sint16>(box.y + 1), static_cast<Uint16>(box.w - 2), static_cast<Uint16>(box.h - 2)};
    SDL_FillRect(dest_surface, &inner, SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b));
    if (auto surface = menu_draw::shade(font, shown, theme.main_text, theme.background))
    {
        SDL_Rect at = {static_cast<Sint16>(box.x + 6), static_cast<Sint16>(box.y + 4), 0, 0};
        SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
    }

    // The keys
    const int grid_y = field_y + line_height + 8 + 16;
    const int cell_w = (SCREEN_WIDTH - side * 2) / COLUMNS;
    const int cell_h = line_height + 14;
    for (int row = 0; row < ROWS; ++row)
    {
        for (int col = 0; col < COLUMNS; ++col)
        {
            const std::string key = key_at(page, row, col);
            if (key.empty())
            {
                continue;
            }
            const bool selected = row == cursor_row && col == cursor_col;
            SDL_Rect cell = {
                static_cast<Sint16>(side + col * cell_w + 2),
                static_cast<Sint16>(grid_y + row * cell_h + 2),
                static_cast<Uint16>(cell_w - 4),
                static_cast<Uint16>(cell_h - 4)
            };
            const SDL_Color fg = selected ? theme.highlight_text : theme.main_text;
            const SDL_Color bg = selected ? theme.highlight_background : theme.background;
            if (selected)
            {
                SDL_FillRect(dest_surface, &cell, SDL_MapRGB(dest_surface->format, bg.r, bg.g, bg.b));
            }
            if (auto surface = menu_draw::shade(font, key, fg, bg))
            {
                SDL_Rect at = {
                    static_cast<Sint16>(cell.x + (cell.w - surface->w) / 2),
                    static_cast<Sint16>(cell.y + (cell.h - surface->h) / 2),
                    0,
                    0
                };
                SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
            }
        }
    }

    menu_draw::draw_hint_bar(
        dest_surface,
        font,
        theme,
        line_height,
        {"A  Type", "B  Delete", "Y  Space", "Select  Keys", "Start  Done", "X  Cancel"}
    );
    return true;
}

bool TextInputView::is_done()
{
    return _is_done;
}

bool TextInputView::is_modal()
{
    return false;
}

void TextInputView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
            cursor_col = (cursor_col + COLUMNS - 1) % COLUMNS;
            break;
        case SW_BTN_RIGHT:
            cursor_col = (cursor_col + 1) % COLUMNS;
            break;
        case SW_BTN_UP:
            cursor_row = (cursor_row + ROWS - 1) % ROWS;
            break;
        case SW_BTN_DOWN:
            cursor_row = (cursor_row + 1) % ROWS;
            break;
        case SW_BTN_A:
            type_key();
            break;
        case SW_BTN_B:
            erase_last();
            break;
        case SW_BTN_Y:
            if (text.size() < max_length)
            {
                text += " ";
            }
            break;
        case SW_BTN_SELECT:
            page = (page + 1) % PAGES;
            break;
        case SW_BTN_L1:
            page = (page + PAGES - 1) % PAGES;
            break;
        case SW_BTN_R1:
            page = (page + 1) % PAGES;
            break;
        case SW_BTN_START:
            _is_done = true;
            mark_completed();
            if (on_done)
            {
                on_done(text);
            }
            return;
        case SW_BTN_X:
        case SW_BTN_MENU:
            _is_done = true;
            return;
        default:
            return;
    }
    _needs_render = true;
}
