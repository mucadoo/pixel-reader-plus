#include <iostream>
#include "./selection_menu.h"

#include "./menu_draw.h"
#include "sys/screen.h"
#include "sys/keymap.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <cstdio>

uint32_t SelectionMenu::num_display_lines() const
{
    if (compact)
    {
        int count = static_cast<int>(entries.size());
        if (count < 1)
        {
            count = 1;
        }
        if (count > 6)
        {
            count = 6;
        }
        return static_cast<uint32_t>(count);
    }
    int reserved = title.empty() ? 0 : line_height * 2;
    int lines = (static_cast<int>(SCREEN_HEIGHT) - reserved) / std::max(1, line_height);
    if (lines < 1)
    {
        lines = 1;
    }
    return static_cast<uint32_t>(lines);
}

uint32_t SelectionMenu::excess_pxl_y() const
{
    if (compact)
    {
        return 0;
    }
    if (!title.empty())
    {
        int usable = static_cast<int>(SCREEN_HEIGHT) - line_height * 2;
        int extra = usable - static_cast<int>(num_display_lines()) * line_height;
        if (extra < 0)
        {
            extra = 0;
        }
        return static_cast<uint32_t>(extra);
    }
    return SCREEN_HEIGHT - num_display_lines() * line_height;
}

SelectionMenu::SelectionMenu(SystemStyling &styling)
    : SelectionMenu({}, styling)
{
}

SelectionMenu::SelectionMenu(std::vector<std::string> entries, SystemStyling &styling)
    : entries(entries),
      styling(styling),
      styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          needs_render = true;
          int new_line_height = detect_line_height(
              this->styling.get_font_name(),
              this->styling.get_font_size()
          ) + line_padding;
          if (new_line_height != line_height)
          {
              line_height = new_line_height;
              set_cursor_pos(cursor_pos);
          }
      })),
      line_height(detect_line_height(
          styling.get_font_name(),
          styling.get_font_size()
      ) + line_padding),
      scroll_throttle(250, 100)
{
}

SelectionMenu::~SelectionMenu()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void SelectionMenu::set_entries(std::vector<std::string> new_entries)
{
    entries = new_entries;
    details.clear();
    set_cursor_pos(0);
    needs_render = true;
}

void SelectionMenu::set_details(std::vector<std::string> new_details)
{
    details = std::move(new_details);
    needs_render = true;
}

void SelectionMenu::set_empty_message(std::string message)
{
    empty_message = std::move(message);
    needs_render = true;
}

void SelectionMenu::set_hints(std::vector<std::string> new_hints)
{
    hints = std::move(new_hints);
    needs_render = true;
}

void SelectionMenu::set_on_selection(std::function<void(uint32_t)> callback)
{
    on_selection = callback;
}

void SelectionMenu::set_on_focus(std::function<void(uint32_t)> callback)
{
    on_focus = callback;
}

void SelectionMenu::set_default_on_keypress(std::function<void(SDLKey, SelectionMenu &)> callback)
{
    default_on_keypress = callback;
}

void SelectionMenu::set_close_on_select()
{
    close_on_select = true;
}

void SelectionMenu::set_title(std::string new_title)
{
    title = std::move(new_title);
    needs_render = true;
}

void SelectionMenu::set_compact(bool enabled)
{
    compact = enabled;
    needs_render = true;
}

void SelectionMenu::set_marker(uint32_t index)
{
    marker_pos = static_cast<int>(index);
    needs_render = true;
}

void SelectionMenu::set_cursor_pos(const std::string &entry)
{
    for (uint32_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i] == entry)
        {
            set_cursor_pos(i);
            break;
        }
    }
}

void SelectionMenu::set_cursor_pos(uint32_t new_cursor_pos)
{
    if (new_cursor_pos >= entries.size())
    {
        new_cursor_pos = 0;
    }

    cursor_pos = new_cursor_pos;
    marquee.reset();
    if (on_focus)
    {
        on_focus(cursor_pos);
    }

    int num_lines = num_display_lines();
    int num_entries = entries.size();
    scroll_pos = std::max(
        0,
        std::min(
            num_entries - num_lines,
            static_cast<int>(new_cursor_pos) - num_lines / 4 - 1
        )
    );

    needs_render = true;
}

void SelectionMenu::close()
{
    _is_done = true;
}

using namespace menu_draw;

bool SelectionMenu::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!needs_render && !force_render)
    {
        return false;
    }
    needs_render = false;

    TTF_Font *loaded_font = styling.get_loaded_font();
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color &fg_color = theme.main_text;
    const SDL_Color &bg_color = theme.background;
    const SDL_Color &hl_bg_color = theme.highlight_background;
    const SDL_Color &hl_text_color = theme.highlight_text;
    SDL_Color quiet = quiet_text(theme);

    uint32_t rect_bg_color = SDL_MapRGB(dest_surface->format, bg_color.r, bg_color.g, bg_color.b);
    uint32_t rect_highlight_color = SDL_MapRGB(dest_surface->format, hl_bg_color.r, hl_bg_color.g, hl_bg_color.b);
    uint32_t rect_fg_color = SDL_MapRGB(dest_surface->format, fg_color.r, fg_color.g, fg_color.b);
    uint32_t rect_hl_text_color = SDL_MapRGB(dest_surface->format, hl_text_color.r, hl_text_color.g, hl_text_color.b);
    uint32_t rect_quiet_color = SDL_MapRGB(dest_surface->format, quiet.r, quiet.g, quiet.b);

    if (compact)
    {
        dim_screen(dest_surface);

        const int pad = 18;
        uint32_t shown = num_display_lines();
        int title_h = title.empty() ? 0 : line_height + 8;
        int footer_h = line_height;
        int panel_h = pad + title_h + static_cast<int>(shown) * line_height + 8 + footer_h + pad;

        int content_w = text_width(loaded_font, title);
        for (uint32_t i = 0; i < shown; ++i)
        {
            uint32_t global_i = i + scroll_pos;
            if (global_i >= entries.size())
            {
                break;
            }
            content_w = std::max(content_w, text_width(loaded_font, entries[global_i]));
        }
        content_w = std::max(content_w, text_width(loaded_font, "A  Choose") + text_width(loaded_font, "B  Back") + 24);
        int panel_w = std::min(static_cast<int>(SCREEN_WIDTH) - 36, std::max(220, content_w + pad * 2 + 16));
        int panel_x = (static_cast<int>(SCREEN_WIDTH) - panel_w) / 2;
        int panel_y = (static_cast<int>(SCREEN_HEIGHT) - panel_h) / 2;
        if (panel_y < 8)
        {
            panel_y = 8;
        }

        SDL_Rect frame = {
            static_cast<Sint16>(panel_x),
            static_cast<Sint16>(panel_y),
            static_cast<Uint16>(panel_w),
            static_cast<Uint16>(panel_h)
        };
        SDL_FillRect(dest_surface, &frame, rect_fg_color);
        frame.x += 1;
        frame.y += 1;
        frame.w -= 2;
        frame.h -= 2;
        SDL_FillRect(dest_surface, &frame, rect_bg_color);

        int y = panel_y + pad;
        if (!title.empty())
        {
            auto heading = shade(loaded_font, title, fg_color, bg_color);
            blit_crop(heading.get(), dest_surface, panel_x + pad, y + std::max(0, (line_height - (heading ? heading->h : 0)) / 2), panel_w - pad * 2);
            y += title_h;
        }

        for (uint32_t i = 0; i < shown; ++i)
        {
            uint32_t global_i = i + scroll_pos;
            if (global_i >= entries.size())
            {
                break;
            }
            bool is_highlighted = global_i == cursor_pos;
            if (is_highlighted)
            {
                SDL_Rect row = {
                    static_cast<Sint16>(panel_x + 8),
                    static_cast<Sint16>(y),
                    static_cast<Uint16>(panel_w - 16),
                    static_cast<Uint16>(line_height)
                };
                SDL_FillRect(dest_surface, &row, rect_highlight_color);
            }
            auto message = shade(
                loaded_font,
                entries[global_i],
                is_highlighted ? hl_text_color : fg_color,
                is_highlighted ? hl_bg_color : bg_color
            );
            int text_y = y + line_padding / 2;
            blit_crop(message.get(), dest_surface, panel_x + pad, text_y, panel_w - pad * 2);
            y += line_height;
        }

        int footer_y = panel_y + panel_h - pad - footer_h;
        auto choose = shade(loaded_font, "A  Choose", quiet, bg_color);
        auto back = shade(loaded_font, "B  Back", quiet, bg_color);
        int hint_y = footer_y + std::max(0, (footer_h - (choose ? choose->h : 0)) / 2);
        blit_crop(choose.get(), dest_surface, panel_x + pad, hint_y, panel_w / 2);
        if (back)
        {
            blit_crop(back.get(), dest_surface, panel_x + panel_w - pad - back->w, hint_y, back->w);
        }
        return true;
    }

    SDL_Rect screen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(dest_surface, &screen, rect_bg_color);

    bool chrome = !title.empty();
    if (chrome)
    {
        char count[32] = "";
        if (!entries.empty())
        {
            std::snprintf(count, sizeof(count), "%u / %u", cursor_pos + 1, static_cast<unsigned>(entries.size()));
        }
        draw_title_bar(dest_surface, loaded_font, theme, line_height, title, count);
    }

    Sint16 x = chrome ? 16 : line_padding;
    Sint16 y = chrome ? static_cast<Sint16>(line_height + excess_pxl_y() / 2) : static_cast<Sint16>(excess_pxl_y() / 2);

    uint32_t num_lines = num_display_lines();
    for (uint32_t i = 0; i < num_lines; ++i)
    {
        uint32_t global_i = i + scroll_pos;
        if (global_i >= entries.size())
        {
            break;
        }

        bool is_highlighted = global_i == cursor_pos;
        int inset = chrome ? 8 : 0;
        if (is_highlighted)
        {
            SDL_Rect row = {
                static_cast<Sint16>(inset),
                y,
                static_cast<Uint16>(SCREEN_WIDTH - inset * 2),
                static_cast<Uint16>(line_height)
            };
            SDL_FillRect(dest_surface, &row, rect_highlight_color);
        }

        if (chrome && marker_pos == static_cast<int>(global_i))
        {
            SDL_Rect mark = {8, y, 4, static_cast<Uint16>(line_height)};
            SDL_FillRect(dest_surface, &mark, is_highlighted ? rect_hl_text_color : rect_fg_color);
        }

        int detail_reserve = 0;
        if (chrome && global_i < details.size() && !details[global_i].empty())
        {
            auto detail = shade(
                loaded_font,
                details[global_i],
                is_highlighted ? hl_text_color : quiet,
                is_highlighted ? hl_bg_color : bg_color
            );
            if (detail)
            {
                int detail_x = static_cast<int>(SCREEN_WIDTH) - 18 - detail->w;
                blit_crop(detail.get(), dest_surface, detail_x, y + line_padding / 2, detail->w);
                detail_reserve = detail->w + 14;
            }
        }

        auto message = shade(
            loaded_font,
            entries[global_i],
            is_highlighted ? hl_text_color : fg_color,
            is_highlighted ? hl_bg_color : bg_color
        );
        const int text_space = static_cast<int>(SCREEN_WIDTH) - x - 12 - detail_reserve;
        int shift = 0;
        if (is_highlighted && chrome)
        {
            // A name too long for its row slides, so all of it can be read.
            marquee.set_overflow(message ? message->w - text_space : 0);
            shift = marquee.offset();
        }
        blit_crop(
            message.get(),
            dest_surface,
            x,
            y + line_padding / 2,
            text_space,
            shift
        );

        y += line_height;
    }

    if (chrome && !empty_message.empty())
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
        int block_h = static_cast<int>(lines.size()) * line_height;
        int top = line_height + (static_cast<int>(SCREEN_HEIGHT) - line_height * 2 - block_h) / 2;
        int rows_bottom = line_height + static_cast<int>(std::min<size_t>(entries.size(), num_lines)) * line_height;
        top = std::max(top, rows_bottom + line_height / 2);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto line = shade(loaded_font, lines[i], i == 0 ? fg_color : quiet, bg_color);
            if (line)
            {
                int line_x = std::max(14, (static_cast<int>(SCREEN_WIDTH) - line->w) / 2);
                blit_crop(line.get(), dest_surface, line_x, top + static_cast<int>(i) * line_height, static_cast<int>(SCREEN_WIDTH) - 28);
            }
        }
    }

    if (chrome)
    {
        draw_hint_bar(dest_surface, loaded_font, theme, line_height, hints);
    }

    return true;
}

bool SelectionMenu::is_modal()
{
    return compact;
}

bool SelectionMenu::tick(uint32_t elapsed_ms)
{
    if (marquee.tick(elapsed_ms))
    {
        needs_render = true;
        return true;
    }
    return false;
}

bool SelectionMenu::is_done()
{
    return _is_done;
}

void SelectionMenu::on_move_down(uint32_t step)
{
    if (cursor_pos < entries.size() - 1)
    {
        cursor_pos = std::min(
            cursor_pos + step,
            static_cast<uint32_t>(entries.size()) - 1
        );

        if (cursor_pos >= scroll_pos + num_display_lines())
        {
            scroll_pos = cursor_pos - num_display_lines() + 1;
        }
        marquee.reset();
        if (on_focus)
        {
            on_focus(cursor_pos);
        }

        needs_render = true;
    }
}

void SelectionMenu::on_move_up(uint32_t step)
{
    if (cursor_pos > 0)
    {
        cursor_pos = cursor_pos <= step ? 0 : cursor_pos - step;
        scroll_pos = std::min(scroll_pos, cursor_pos);
        marquee.reset();

        if (on_focus)
        {
            on_focus(cursor_pos);
        }

        needs_render = true;
    }
}

void SelectionMenu::on_select_entry()
{
    if (!entries.empty() && on_selection)
    {
        on_selection(cursor_pos);
    }

    if (close_on_select)
    {
        _is_done = true;
    }
}

void SelectionMenu::on_keypress(SDLKey key)
{
    switch (key) {
        case SW_BTN_UP:
            on_move_up(1);
            break;
        case SW_BTN_DOWN:
            on_move_down(1);
            break;
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            {
                auto [l_key, r_key] = get_shoulder_keymap_lr(
                    styling.get_shoulder_keymap()
                );

                if (key == l_key)
                {
                    key = SW_BTN_LEFT;
                }
                else if (key == r_key)
                {
                    key = SW_BTN_RIGHT;
                }
            }
            // fallthrough
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            if (key == SW_BTN_LEFT)
            {
                on_move_up(num_display_lines() / 2);
            }
            else if (key == SW_BTN_RIGHT)
            {
                on_move_down(num_display_lines() / 2);
            }
            break;
        case SW_BTN_A:
            on_select_entry();
            break;
        case SW_BTN_B:
            _is_done = true;
            break;
        default:
            if (default_on_keypress)
            {
                default_on_keypress(key, *this);
            }
            break;
    }
}

void SelectionMenu::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    switch (key) {
        case SW_BTN_UP:
        case SW_BTN_DOWN:
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            if (scroll_throttle(held_time_ms))
            {
                on_keypress(key);
            }
            break;
        default:
            break;
    }
}
