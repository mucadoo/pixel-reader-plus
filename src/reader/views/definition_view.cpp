#include "./definition_view.h"

#include "reader/system_styling.h"
#include "reader/text_style.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"
#include "util/utf8.h"
#include "reader/views/menu_draw.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

DefinitionView::DefinitionView(std::string title, std::string body, SystemStyling &styling)
    : title(std::move(title))
    , body(std::move(body))
    , font_name(styling.get_font_name())
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
}

DefinitionView::~DefinitionView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

namespace
{

std::vector<std::string> wrap_body(TTF_Font *font, const std::string &text, int max_w)
{
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    auto flush_word = [&]() {
        if (word.empty())
        {
            return;
        }
        int w = 0;
        int h = 0;
        TTF_SizeUTF8(font, word.c_str(), &w, &h);
        if (w > max_w)
        {
            // A word wider than a line, such as a long path: break it by letters.
            if (!line.empty())
            {
                lines.push_back(line);
                line.clear();
            }
            std::string piece;
            for (size_t i = 0; i < word.size();)
            {
                const char *next = utf8_step(word.c_str() + i);
                std::string grown = piece + std::string(word.c_str() + i, next);
                TTF_SizeUTF8(font, grown.c_str(), &w, &h);
                if (w > max_w && !piece.empty())
                {
                    lines.push_back(piece);
                    piece.clear();
                    grown = std::string(word.c_str() + i, next);
                }
                piece = std::move(grown);
                i = static_cast<size_t>(next - word.c_str());
            }
            line = piece;
            word.clear();
            return;
        }
        std::string trial = line.empty() ? word : line + " " + word;
        TTF_SizeUTF8(font, trial.c_str(), &w, &h);
        if (w > max_w && !line.empty())
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            line = std::move(trial);
        }
        word.clear();
    };

    for (size_t i = 0; i < text.size();)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == ' ' || c == '\n')
        {
            flush_word();
            if (c == '\n')
            {
                lines.push_back(line); // an empty line keeps a blank row
                line.clear();
            }
            ++i;
            continue;
        }
        const char *next = utf8_step(text.c_str() + i);
        word.append(text.c_str() + i, next);
        i = static_cast<size_t>(next - text.c_str());
    }
    flush_word();
    if (!line.empty())
    {
        lines.push_back(line);
    }
    if (lines.empty())
    {
        lines.push_back("");
    }
    return lines;
}

} // namespace

void DefinitionView::build_layout()
{
    TTF_Font *font = cached_load_font(font_name, styling.get_font_size());
    lines = wrap_body(font, body, SCREEN_WIDTH - 24);
    words.clear();
    for (uint32_t i = 0; i < lines.size(); ++i)
    {
        const std::string &line = lines[i];
        uint32_t at = 0;
        while (at < line.size())
        {
            while (at < line.size() && line[at] == ' ')
            {
                ++at;
            }
            uint32_t begin = at;
            while (at < line.size() && line[at] != ' ')
            {
                ++at;
            }
            if (at > begin)
            {
                words.push_back({i, begin, at});
            }
        }
    }
    if (picked >= words.size())
    {
        picked = words.empty() ? 0 : static_cast<uint32_t>(words.size() - 1);
    }
}

int DefinitionView::word_x(uint32_t index) const
{
    if (index >= words.size())
    {
        return 0;
    }
    TTF_Font *font = cached_load_font(font_name, styling.get_font_size());
    const WordPos &word = words[index];
    const std::string before = lines[word.line].substr(0, word.begin);
    int w = 0;
    int h = 0;
    if (!before.empty())
    {
        TTF_SizeUTF8(font, before.c_str(), &w, &h);
    }
    return w;
}

void DefinitionView::move_pick(SDLKey key)
{
    if (words.empty())
    {
        return;
    }
    if (key == SW_BTN_LEFT)
    {
        picked = picked > 0 ? picked - 1 : picked;
    }
    else if (key == SW_BTN_RIGHT)
    {
        picked = picked + 1 < words.size() ? picked + 1 : picked;
    }
    else
    {
        // The word on the next or previous line that sits closest across.
        const int direction = key == SW_BTN_UP ? -1 : 1;
        const int target_line = static_cast<int>(words[picked].line) + direction;
        const int x = word_x(picked);
        int best = -1;
        int best_distance = 1 << 30;
        for (uint32_t i = 0; i < words.size(); ++i)
        {
            if (static_cast<int>(words[i].line) != target_line)
            {
                continue;
            }
            const int distance = std::abs(word_x(i) - x);
            if (distance < best_distance)
            {
                best_distance = distance;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0)
        {
            picked = static_cast<uint32_t>(best);
        }
    }
    // Keep the picked word on the screen
    const uint32_t line = words[picked].line;
    if (line < scroll_pos)
    {
        scroll_pos = line;
    }
    else if (line >= scroll_pos + visible_lines)
    {
        scroll_pos = line + 1 - visible_lines;
    }
    _needs_render = true;
}

bool DefinitionView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }

    TTF_Font *font = cached_load_font(font_name, styling.get_font_size());
    const auto &theme = styling.get_loaded_color_theme();
    SDL_Rect screen_rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(
        dest_surface,
        &screen_rect,
        SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b)
    );

    const int pad = 12;
    int line_h = detect_line_height(font) + 4;
    int y = pad;

    auto blit_line = [&](const std::string &text, SDL_Color color) {
        if (text.empty() || y > SCREEN_HEIGHT - pad)
        {
            y += line_h;
            return;
        }
        SDL_Surface *surface = TTF_RenderUTF8_Shaded(font, text.c_str(), color, theme.background);
        if (surface)
        {
            SDL_Rect dest = {static_cast<Sint16>(pad), static_cast<Sint16>(y), 0, 0};
            SDL_BlitSurface(surface, nullptr, dest_surface, &dest);
            SDL_FreeSurface(surface);
        }
        y += line_h;
    };

    blit_line(title, theme.main_text);
    y += line_h / 2;

    build_layout();
    const int bottom_reserved = on_lookup ? line_h : 0;
    visible_lines = static_cast<uint32_t>(std::max(1, (SCREEN_HEIGHT - y - pad - bottom_reserved) / line_h));
    if (scroll_pos > lines.size())
    {
        scroll_pos = 0;
    }
    if (scroll_pos + visible_lines > lines.size() && lines.size() > visible_lines)
    {
        scroll_pos = static_cast<uint32_t>(lines.size() - visible_lines);
    }
    const int first_y = y;
    for (uint32_t i = scroll_pos; i < lines.size() && i < scroll_pos + visible_lines; ++i)
    {
        blit_line(lines[i], theme.secondary_text);
    }

    if (picking && picked < words.size() && words[picked].line >= scroll_pos && words[picked].line < scroll_pos + visible_lines)
    {
        const WordPos &word = words[picked];
        const std::string text = lines[word.line].substr(word.begin, word.end - word.begin);
        SDL_Surface *surface = TTF_RenderUTF8_Shaded(font, text.c_str(), theme.highlight_text, theme.highlight_background);
        if (surface)
        {
            SDL_Rect dest = {
                static_cast<Sint16>(pad + word_x(picked)),
                static_cast<Sint16>(first_y + static_cast<int>(word.line - scroll_pos) * line_h),
                0,
                0
            };
            SDL_BlitSurface(surface, nullptr, dest_surface, &dest);
            SDL_FreeSurface(surface);
        }
    }
    if (on_lookup)
    {
        menu_draw::draw_hint_bar(
            dest_surface,
            font,
            theme,
            line_h,
            picking ? std::vector<std::string> {"Arrows  Move", "A  Look up", "B  Back"}
                    : std::vector<std::string> {"A  Pick a word", "B  Close"}
        );
    }

    _needs_render = false;
    return true;
}

bool DefinitionView::is_done()
{
    return _is_done;
}

bool DefinitionView::is_modal()
{
    return false;
}

void DefinitionView::on_keypress(SDLKey key)
{
    if (on_lookup)
    {
        if (lines.empty())
        {
            build_layout();
        }
        if (picking)
        {
            if (key == SW_BTN_LEFT || key == SW_BTN_RIGHT || key == SW_BTN_UP || key == SW_BTN_DOWN)
            {
                move_pick(key);
            }
            else if (key == SW_BTN_A)
            {
                if (picked < words.size())
                {
                    const WordPos &word = words[picked];
                    std::string text = lines[word.line].substr(word.begin, word.end - word.begin);
                    // Marks around the word are not part of it.
                    auto is_mark = [](unsigned char c) { return c < 0x80 && !std::isalnum(c); };
                    while (!text.empty() && is_mark(static_cast<unsigned char>(text.back())) && text.back() != '\'')
                    {
                        text.pop_back();
                    }
                    while (!text.empty() && is_mark(static_cast<unsigned char>(text.front())))
                    {
                        text.erase(0, 1);
                    }
                    if (!text.empty())
                    {
                        on_lookup(lookup_lowercase(text));
                    }
                }
            }
            else
            {
                picking = false;
                _needs_render = true;
            }
            return;
        }
        if (key == SW_BTN_A && !words.empty())
        {
            picking = true;
            // Start on the first word that is on the screen
            for (uint32_t i = 0; i < words.size(); ++i)
            {
                if (words[i].line >= scroll_pos)
                {
                    picked = i;
                    break;
                }
            }
            _needs_render = true;
            return;
        }
    }
    if (key == SW_BTN_UP)
    {
        if (scroll_pos > 0)
        {
            --scroll_pos;
            _needs_render = true;
        }
        return;
    }
    if (key == SW_BTN_DOWN)
    {
        ++scroll_pos;
        _needs_render = true;
        return;
    }
    _is_done = true;
}