#include "./guide_view.h"

#include "reader/system_styling.h"
#include "reader/views/menu_draw.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{

// A button (or a few) and what it does. A row with no buttons is a plain note.
struct Row
{
    const char *keys;
    const char *text;
};

struct Section
{
    const char *name;
    std::vector<Row> rows;
};

const std::vector<Section> &sections()
{
    static const std::vector<Section> list = {
        {"Reading", {
            {"Up / Down", "Scroll a line in Scroll mode. Hold to keep going. In Page mode they turn a page."},
            {"Left / Right", "Turn a screen in Scroll mode, or a page in Page mode."},
            {"L1 / R1", "Turn a page, like Left and Right."},
            {"L2 / R2", "Go to the previous or next chapter. While a search is open they step between the matches."},
            {"A", "Show or hide the progress line."},
            {"Start", "Add a bookmark on this page, or remove the one that is there."},
            {"Y", "Select a word near the middle of the page."},
            {"X", "Open the settings."},
            {"Select", "Open the book menu."},
            {"B", "Close the book and go back to the list."},
            {"Menu", "Quit the reader. Your place is saved."},
            {"", "Turning forward on the last page shows the end of the book: the next book, a star rating and your time."},
        }},
        {"Select a word", {
            {"D-pad", "Move the cursor. Hold to repeat. At the edge of the page it turns the page, in Page mode up and down too."},
            {"Start", "Keep this word and add the words you move through."},
            {"X", "Grow the selection to the sentence, then the paragraph, then back to the word."},
            {"A", "Look up, Search book, Highlight, Underline or Strike out. On a link: Show note and Go to link."},
            {"B / Y", "Leave selection."},
            {"", "Commas, periods and quotes stay outside the word. Look up and Search use the language of the book."},
        }},
        {"Book menu", {
            {"A / B", "A opens a group, B goes back. Press Select again to close the menu."},
            {"Go to", "Chapters (Y finds one by letters), Go to %, Go to page, Book map, Skim the book and Jump back."},
            {"Search", "Search in book. After you pick a result the words stay marked: L2 and R2 step between matches, and Clear search ends it."},
            {"Marks", "Bookmarks (A goes, X deletes, Y renames), Highlights (Y adds a note, Start shows one kind), Next and Previous mark, export and import, Highlight tools and Undo delete."},
            {"Words", "Look up a word, Look up in..., Browse the dictionary, the Word list (A shows, X deletes), Words of this book and flash card review."},
            {"Pictures", "View picture opens one full screen: A zooms, Y fits, the D-pad moves, B closes. The gallery shows every picture of the book."},
            {"Tools", "Reading history, Reading stats, Reset this book's layout and this guide."},
            {"Chapters", "Start folds or unfolds a part, X folds or unfolds every part."},
            {"Go to %", "Left and right move 1, up and down move 10, A goes."},
        }},
        {"Book list", {
            {"Up / Down", "Move one row."},
            {"Left / Right", "Move half a screen. In the cover grid they move one cover."},
            {"L1 / R1", "Move a screen. L2 and R2 jump to the next or previous first letter."},
            {"A", "Open the book or folder."},
            {"B", "Go up one folder."},
            {"X", "Open the settings."},
            {"Y", "Options for the entry: mark books, new folder, paste, properties, favorite, status, search, collections, library tools, rename and delete. Y again closes them."},
            {"Select", "Switch between the list and the cover grid."},
            {"Start", "Change the order: name, title, author, series, recent, size, newest, most read."},
            {"Menu", "Quit the reader."},
            {"", "Continue at the top of a folder opens the book you read last. A star in front of a title marks a favorite."},
        }},
        {"Settings", {
            {"A / B", "A opens a group, B goes back or closes the settings."},
            {"X", "Close the settings from any screen."},
            {"Up / Down", "Change the row."},
            {"Left / Right", "Change the choice on the row."},
            {"", "The groups are Theme and colors, Page layout, Text, Reading, Bar and clock, Library and power and Controls."},
            {"", "Inside a book the choices belong to that book alone. In the book list they set the defaults."},
        }},
        {"Pages", {
            {"Scroll", "The progress line shows the percent, and it follows the text."},
            {"Page", "It shows the page and how many pages there are. The pages are counted from the start first."},
            {"Book / Chapter", "Choose whether progress covers the whole book or the current chapter. In Chapter mode, turning past the end of a chapter goes into the next one."},
            {"", "A new chapter, a title page and a picture that does not fit start at the top of a page. A page never starts on a blank line."},
        }},
    };
    return list;
}

} // namespace

GuideView::GuideView(SystemStyling &styling)
    : styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
}

GuideView::~GuideView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void GuideView::change_section(int direction)
{
    const int count = static_cast<int>(sections().size());
    section = static_cast<uint32_t>((static_cast<int>(section) + count + direction) % count);
    first_row = 0;
    _needs_render = true;
}

bool GuideView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }

    TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color quiet = menu_draw::quiet_text(theme);
    SDL_Rect screen_rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(
        dest_surface,
        &screen_rect,
        SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b)
    );

    const int line_h = detect_line_height(font) + 8;
    const Section &current = sections()[section];
    menu_draw::draw_title_bar(
        dest_surface,
        font,
        theme,
        line_h,
        std::string("Controls: ") + current.name,
        std::to_string(section + 1) + " / " + std::to_string(sections().size())
    );

    // The button column is as wide as its longest label, up to a third of the screen.
    const int pad = 14;
    int key_w = 0;
    for (const Row &row : current.rows)
    {
        key_w = std::max(key_w, menu_draw::text_width(font, row.keys));
    }
    key_w = std::min(key_w + 16, static_cast<int>(SCREEN_WIDTH) / 3);
    const int text_x = pad + key_w + 12;
    const int text_w = static_cast<int>(SCREEN_WIDTH) - text_x - pad - 6;
    const int note_w = static_cast<int>(SCREEN_WIDTH) - 2 * pad - 6;
    const int small_h = line_h - 4;
    const int row_gap = 8;

    // Every row's lines, and how tall it is.
    std::vector<std::vector<std::string>> wrapped;
    std::vector<int> heights;
    for (const Row &row : current.rows)
    {
        const bool note = row.keys[0] == '\0';
        wrapped.push_back(menu_draw::wrap_lines(font, row.text, note ? note_w : text_w, 12));
        heights.push_back(std::max(line_h, static_cast<int>(wrapped.back().size()) * small_h) + row_gap);
    }

    const int top = line_h + 10;
    const int bottom = static_cast<int>(SCREEN_HEIGHT) - line_h - 4;
    const int room = bottom - top;

    // The last row is reachable: scrolling stops when the rest fits.
    uint32_t last_start = 0;
    {
        int used = 0;
        last_start = static_cast<uint32_t>(heights.size());
        while (last_start > 0 && used + heights[last_start - 1] <= room)
        {
            used += heights[last_start - 1];
            --last_start;
        }
    }
    first_row = std::min(first_row, last_start);

    int y = top;
    uint32_t shown_to = first_row;
    for (uint32_t i = first_row; i < current.rows.size(); ++i)
    {
        if (y + heights[i] - row_gap > bottom)
        {
            break;
        }
        const Row &row = current.rows[i];
        const bool note = row.keys[0] == '\0';
        if (!note)
        {
            auto cap = menu_draw::shade(font, row.keys, theme.highlight_text, theme.highlight_background);
            if (cap)
            {
                SDL_Rect pill = {
                    static_cast<Sint16>(pad),
                    static_cast<Sint16>(y),
                    static_cast<Uint16>(key_w),
                    static_cast<Uint16>(line_h - 4)
                };
                SDL_FillRect(
                    dest_surface,
                    &pill,
                    SDL_MapRGB(dest_surface->format, theme.highlight_background.r, theme.highlight_background.g, theme.highlight_background.b)
                );
                const int cap_x = pad + std::max(0, (key_w - cap->w) / 2);
                menu_draw::blit_crop(cap.get(), dest_surface, cap_x, y + std::max(0, (line_h - 4 - cap->h) / 2), key_w);
            }
        }
        for (size_t l = 0; l < wrapped[i].size(); ++l)
        {
            auto text = menu_draw::shade(font, wrapped[i][l], note ? quiet : theme.main_text, theme.background);
            if (text)
            {
                menu_draw::blit_crop(
                    text.get(),
                    dest_surface,
                    note ? pad : text_x,
                    y + static_cast<int>(l) * small_h + std::max(0, (small_h - text->h) / 2),
                    note ? note_w : text_w
                );
            }
        }
        y += heights[i];
        shown_to = i + 1;
    }

    // A thin bar at the right edge shows where this screen is in the topic.
    if (last_start > 0)
    {
        const int track_h = room;
        const int total = static_cast<int>(current.rows.size());
        const int thumb_h = std::max(8, track_h * static_cast<int>(shown_to - first_row) / std::max(1, total));
        const int thumb_y = top + (track_h - thumb_h) * static_cast<int>(first_row) / static_cast<int>(last_start);
        SDL_Rect track = {static_cast<Sint16>(SCREEN_WIDTH - 5), static_cast<Sint16>(top), 2, static_cast<Uint16>(track_h)};
        SDL_FillRect(dest_surface, &track, SDL_MapRGB(dest_surface->format, quiet.r, quiet.g, quiet.b));
        SDL_Rect thumb = {static_cast<Sint16>(SCREEN_WIDTH - 6), static_cast<Sint16>(thumb_y), 4, static_cast<Uint16>(thumb_h)};
        SDL_FillRect(
            dest_surface,
            &thumb,
            SDL_MapRGB(dest_surface->format, theme.main_text.r, theme.main_text.g, theme.main_text.b)
        );
    }

    menu_draw::draw_hint_bar(dest_surface, font, theme, line_h, {"Left / Right  Topic", "Up / Down  Scroll", "B  Close"});

    _needs_render = false;
    return true;
}

bool GuideView::is_done()
{
    return _is_done;
}

bool GuideView::is_modal()
{
    return false;
}

void GuideView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
        case SW_BTN_L1:
        case SW_BTN_L2:
            change_section(-1);
            break;
        case SW_BTN_RIGHT:
        case SW_BTN_R1:
        case SW_BTN_R2:
            change_section(1);
            break;
        case SW_BTN_UP:
            if (first_row > 0)
            {
                --first_row;
                _needs_render = true;
            }
            break;
        case SW_BTN_DOWN:
            ++first_row; // render() holds it at the last reachable row
            _needs_render = true;
            break;
        case SW_BTN_A:
        case SW_BTN_B:
            _is_done = true;
            break;
        default:
            break;
    }
}
