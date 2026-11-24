#ifndef MENU_DRAW_H_
#define MENU_DRAW_H_

#include "reader/color_theme.h"
#include "util/sdl_pointer.h"

#include <SDL/SDL_ttf.h>
#include <SDL/SDL_video.h>

#include <cstdint>
#include <string>
#include <vector>

// Pieces shared by the menus and the book grid, so they look alike.
namespace menu_draw
{

// Main text pulled toward the background, for hints and counters.
SDL_Color quiet_text(const ColorTheme &theme);

surface_unique_ptr shade(TTF_Font *font, const std::string &text, SDL_Color fg, SDL_Color bg);

// Copy src to dest at (x, y), starting src_x pixels into src and at most max_w wide.
void blit_crop(SDL_Surface *src, SDL_Surface *dest, int x, int y, int max_w, int src_x = 0);

int text_width(TTF_Font *font, const std::string &text);

// Darken the whole screen, as a backdrop for a small panel.
void dim_screen(SDL_Surface *dest);

// The text cut to fit, ending in "..." when something was removed.
std::string ellipsize(TTF_Font *font, const std::string &text, int max_w);

// Greedy word wrap. Words wider than a line are split. The last line ends in
// "..." when the text does not fit in max_lines.
std::vector<std::string> wrap_lines(TTF_Font *font, const std::string &text, int max_w, size_t max_lines);

// Title on the left, counter on the right, and a rule under them.
void draw_title_bar(
    SDL_Surface *dest,
    TTF_Font *font,
    const ColorTheme &theme,
    int line_height,
    const std::string &title,
    const std::string &counter
);

// A rule and button hints along the bottom. The first hint sits left and the
// last right, the others between. When they do not fit, middle ones are dropped.
void draw_hint_bar(
    SDL_Surface *dest,
    TTF_Font *font,
    const ColorTheme &theme,
    int line_height,
    const std::vector<std::string> &hints
);

// Slides a line of text that is too wide for its space, so all of it can be
// read: it waits, scrolls to the end, waits again, then starts over.
class Marquee
{
    uint32_t elapsed_ms = 0;
    int overflow_px = 0;

public:
    // Pixels by which the text is wider than its space. 0 when it fits.
    void set_overflow(int pixels);
    // Start again from the beginning, for example when the cursor moves.
    void reset();
    // Pixels to shift the text left right now.
    int offset() const;
    // Advance the clock. True when the text has to be drawn again.
    bool tick(uint32_t ms);
};

} // namespace menu_draw

#endif
