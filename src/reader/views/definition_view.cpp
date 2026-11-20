#include "./definition_view.h"

#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"
#include "util/utf8.h"

#include <algorithm>
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
        std::string trial = line.empty() ? word : line + " " + word;
        int w = 0;
        int h = 0;
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
            if (c == '\n' && !line.empty())
            {
                lines.push_back(line);
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
    int max_w = SCREEN_WIDTH - pad * 2;
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

    std::vector<std::string> lines = wrap_body(font, body, max_w);
    uint32_t max_lines = static_cast<uint32_t>(std::max(1, (SCREEN_HEIGHT - y - pad) / line_h));
    if (scroll_pos > lines.size())
    {
        scroll_pos = 0;
    }
    if (scroll_pos + max_lines > lines.size() && lines.size() > max_lines)
    {
        scroll_pos = static_cast<uint32_t>(lines.size() - max_lines);
    }
    for (uint32_t i = scroll_pos; i < lines.size() && i < scroll_pos + max_lines; ++i)
    {
        blit_line(lines[i], theme.secondary_text);
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
