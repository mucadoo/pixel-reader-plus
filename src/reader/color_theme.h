#ifndef COLOR_THEME_H_
#define COLOR_THEME_H_

#include "SDL/SDL_video.h"

struct ColorTheme
{
    SDL_Color background;
    SDL_Color main_text;
    SDL_Color secondary_text;
    SDL_Color highlight_background;
    SDL_Color highlight_text;
};

// A color moved away from the background by a share of the way to white or black, whichever it is
// closer to being: dark text gets darker and light text gets lighter.
inline SDL_Color push_from_background(SDL_Color color, SDL_Color background, int percent)
{
    const int color_light = (color.r * 299 + color.g * 587 + color.b * 114) / 1000;
    const int background_light = (background.r * 299 + background.g * 587 + background.b * 114) / 1000;
    const int target = color_light >= background_light ? 255 : 0;
    const auto move = [&](Uint8 channel) {
        return static_cast<Uint8>(channel + (target - channel) * percent / 100);
    };
    return SDL_Color {move(color.r), move(color.g), move(color.b), color.unused};
}

// Contrast steps: 0 as the theme has it, 1 stronger text, 2 the strongest.
inline ColorTheme apply_text_contrast(ColorTheme theme, int level)
{
    const int percent = level >= 2 ? 80 : (level == 1 ? 40 : 0);
    if (percent > 0)
    {
        theme.main_text = push_from_background(theme.main_text, theme.background, percent);
        theme.secondary_text = push_from_background(theme.secondary_text, theme.background, percent);
    }
    return theme;
}

inline const char *text_contrast_label(int level)
{
    return level >= 2 ? "Highest" : (level == 1 ? "High" : "Normal");
}

#endif
