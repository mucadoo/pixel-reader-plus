#include "./menu_draw.h"

#include "sys/screen.h"
#include "util/utf8.h"

#include <algorithm>

namespace menu_draw
{

SDL_Color quiet_text(const ColorTheme &theme)
{
    return SDL_Color {
        static_cast<Uint8>((theme.main_text.r * 2 + theme.background.r) / 3),
        static_cast<Uint8>((theme.main_text.g * 2 + theme.background.g) / 3),
        static_cast<Uint8>((theme.main_text.b * 2 + theme.background.b) / 3),
        0
    };
}

surface_unique_ptr shade(TTF_Font *font, const std::string &text, SDL_Color fg, SDL_Color bg)
{
    if (!font || text.empty())
    {
        return nullptr;
    }
    return surface_unique_ptr { TTF_RenderUTF8_Shaded(font, text.c_str(), fg, bg) };
}

void blit_crop(SDL_Surface *src, SDL_Surface *dest, int x, int y, int max_w, int src_x)
{
    if (!src || max_w <= 0 || src_x >= src->w)
    {
        return;
    }
    if (src_x < 0)
    {
        src_x = 0;
    }
    int width = std::min(src->w - src_x, max_w);
    SDL_Rect src_rect = {
        static_cast<Sint16>(src_x),
        0,
        static_cast<Uint16>(width),
        static_cast<Uint16>(src->h)
    };
    SDL_Rect dst = {static_cast<Sint16>(x), static_cast<Sint16>(y), 0, 0};
    SDL_BlitSurface(src, &src_rect, dest, &dst);
}

int text_width(TTF_Font *font, const std::string &text)
{
    int w = 0;
    int h = 0;
    if (font && !text.empty())
    {
        TTF_SizeUTF8(font, text.c_str(), &w, &h);
    }
    return w;
}

void dim_screen(SDL_Surface *dest)
{
    surface_unique_ptr mask { SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0) };
    if (!mask)
    {
        return;
    }
    SDL_SetAlpha(mask.get(), SDL_SRCALPHA, 140);
    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(mask.get(), &rect, SDL_MapRGB(mask->format, 0, 0, 0));
    SDL_BlitSurface(mask.get(), nullptr, dest, &rect);
}

std::string ellipsize(TTF_Font *font, const std::string &text, int max_w)
{
    if (text_width(font, text) <= max_w)
    {
        return text;
    }

    const std::string dots = "...";
    int budget = max_w - text_width(font, dots);
    if (budget <= 0)
    {
        return dots;
    }

    std::string out;
    const char *p = text.c_str();
    while (*p)
    {
        const char *next = utf8_step(p);
        std::string trial = out + std::string(p, next);
        if (text_width(font, trial) > budget)
        {
            break;
        }
        out = std::move(trial);
        p = next;
    }
    return out + dots;
}

std::vector<std::string> wrap_lines(TTF_Font *font, const std::string &text, int max_w, size_t max_lines)
{
    std::vector<std::string> lines;
    if (max_lines == 0 || max_w <= 0)
    {
        return lines;
    }

    // Split into words, breaking any word that cannot fit on a line by itself.
    std::vector<std::string> words;
    {
        std::string word;
        auto flush = [&]() {
            if (word.empty())
            {
                return;
            }
            if (text_width(font, word) <= max_w)
            {
                words.push_back(word);
            }
            else
            {
                std::string chunk;
                const char *p = word.c_str();
                while (*p)
                {
                    const char *next = utf8_step(p);
                    std::string trial = chunk + std::string(p, next);
                    if (!chunk.empty() && text_width(font, trial) > max_w)
                    {
                        words.push_back(chunk);
                        chunk.clear();
                        trial = std::string(p, next);
                    }
                    chunk = std::move(trial);
                    p = next;
                }
                if (!chunk.empty())
                {
                    words.push_back(chunk);
                }
            }
            word.clear();
        };
        for (char c : text)
        {
            if (c == ' ' || c == '\n' || c == '\t')
            {
                flush();
            }
            else
            {
                word.push_back(c);
            }
        }
        flush();
    }

    std::string line;
    for (const auto &word : words)
    {
        std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && text_width(font, trial) > max_w)
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            line = std::move(trial);
        }
    }
    if (!line.empty())
    {
        lines.push_back(line);
    }

    if (lines.size() > max_lines)
    {
        std::string rest;
        for (size_t i = max_lines - 1; i < lines.size(); ++i)
        {
            rest += (rest.empty() ? "" : " ") + lines[i];
        }
        lines.resize(max_lines);
        lines.back() = ellipsize(font, rest, max_w);
    }
    return lines;
}

void draw_title_bar(
    SDL_Surface *dest,
    TTF_Font *font,
    const ColorTheme &theme,
    int line_height,
    const std::string &title,
    const std::string &counter
)
{
    SDL_Color quiet = quiet_text(theme);

    auto heading = shade(font, title, theme.main_text, theme.background);
    blit_crop(
        heading.get(),
        dest,
        14,
        std::max(0, (line_height - (heading ? heading->h : 0)) / 2),
        SCREEN_WIDTH / 2
    );

    if (!counter.empty())
    {
        auto count_surface = shade(font, counter, quiet, theme.background);
        if (count_surface)
        {
            blit_crop(
                count_surface.get(),
                dest,
                static_cast<int>(SCREEN_WIDTH) - 14 - count_surface->w,
                std::max(0, (line_height - count_surface->h) / 2),
                count_surface->w
            );
        }
    }

    SDL_Rect rule = {0, static_cast<Sint16>(line_height - 1), SCREEN_WIDTH, 1};
    SDL_FillRect(dest, &rule, SDL_MapRGB(dest->format, quiet.r, quiet.g, quiet.b));
}

void draw_hint_bar(
    SDL_Surface *dest,
    TTF_Font *font,
    const ColorTheme &theme,
    int line_height,
    const std::vector<std::string> &hints
)
{
    SDL_Color quiet = quiet_text(theme);
    int footer_y = static_cast<int>(SCREEN_HEIGHT) - line_height;
    SDL_Rect rule = {0, static_cast<Sint16>(footer_y), SCREEN_WIDTH, 1};
    SDL_FillRect(dest, &rule, SDL_MapRGB(dest->format, quiet.r, quiet.g, quiet.b));

    std::vector<surface_unique_ptr> surfaces;
    for (const auto &text : hints)
    {
        auto hint = shade(font, text, quiet, theme.background);
        if (hint)
        {
            surfaces.push_back(std::move(hint));
        }
    }

    const int min_gap = 14;
    const int span = static_cast<int>(SCREEN_WIDTH) - 28;
    auto total_width = [&surfaces]() {
        int total = 0;
        for (const auto &hint : surfaces)
        {
            total += hint->w;
        }
        return total;
    };
    while (surfaces.size() > 2 &&
           span - total_width() < min_gap * static_cast<int>(surfaces.size() - 1))
    {
        surfaces.erase(surfaces.begin() + 1);
    }
    if (surfaces.empty())
    {
        return;
    }

    int gaps = static_cast<int>(surfaces.size()) - 1;
    int gap = gaps > 0 ? (span - total_width()) / gaps : 0;
    int x = 14;
    for (size_t i = 0; i < surfaces.size(); ++i)
    {
        const auto &hint = surfaces[i];
        int hint_x = x;
        if (gaps > 0 && i + 1 == surfaces.size())
        {
            hint_x = static_cast<int>(SCREEN_WIDTH) - 14 - hint->w;
        }
        int hint_y = footer_y + std::max(0, (line_height - hint->h) / 2);
        blit_crop(hint.get(), dest, hint_x, hint_y, hint->w);
        x += hint->w + gap;
    }
}

namespace
{

constexpr uint32_t MARQUEE_START_DELAY_MS = 800;
constexpr uint32_t MARQUEE_END_PAUSE_MS = 900;
constexpr uint32_t MARQUEE_PIXELS_PER_SEC = 45;

uint32_t scroll_time_ms(int overflow_px)
{
    return static_cast<uint32_t>(overflow_px) * 1000 / MARQUEE_PIXELS_PER_SEC;
}

} // namespace

void Marquee::set_overflow(int pixels)
{
    pixels = std::max(0, pixels);
    if (pixels != overflow_px)
    {
        overflow_px = pixels;
        elapsed_ms = 0;
    }
}

void Marquee::reset()
{
    elapsed_ms = 0;
    overflow_px = 0;
}

int Marquee::offset() const
{
    if (overflow_px <= 0)
    {
        return 0;
    }

    const uint32_t scroll_ms = scroll_time_ms(overflow_px);
    const uint32_t cycle = MARQUEE_START_DELAY_MS + scroll_ms + MARQUEE_END_PAUSE_MS;
    const uint32_t t = elapsed_ms % cycle;
    if (t < MARQUEE_START_DELAY_MS)
    {
        return 0;
    }
    if (t < MARQUEE_START_DELAY_MS + scroll_ms)
    {
        return static_cast<int>((t - MARQUEE_START_DELAY_MS) * MARQUEE_PIXELS_PER_SEC / 1000);
    }
    return overflow_px;
}

bool Marquee::tick(uint32_t ms)
{
    if (overflow_px <= 0)
    {
        return false;
    }
    int before = offset();
    elapsed_ms += ms;
    return offset() != before;
}

} // namespace menu_draw
