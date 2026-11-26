#include "./color_theme_def.h"
#include "./config.h"

#include <string>
#include <utility>
#include <vector>

namespace
{

std::vector<std::pair<std::string, ColorTheme>> theme_defs = {
    {
        "night_contrast",
        {
            {0, 0, 0, 0},       // background
            {240, 240, 240, 0}, // main text
            {96, 96, 96, 0},    // secondary text
            {150, 38, 200, 0},  // highlight background
            {240, 240, 240, 0}, // highlight text
        }
    },
    {
        "light_contrast",
        {
            {255, 255, 255, 0}, // background
            {0, 0, 0, 0},       // main text
            {160, 160, 160, 0}, // secondary text
            {163, 81, 200, 0},  // highlight background
            {250, 250, 250, 0}, // highlight text
        }
    },
    {
        "light_sepia",
        {
            {250, 240, 220, 0}, // background
            {0, 0, 0, 0},    // main text
            {160, 160, 160, 0}, // secondary text
            {163, 81, 200, 0},  // highlight background
            {250, 240, 220, 0}, // highlight text
        }
    },
    {
        "vampire",
        {
            {0, 0, 0, 0},   // background
            {192, 0, 0, 0}, // main text
            {96, 0, 0, 0},  // secondary text
            {192, 0, 0, 0}, // highlight background
            {0, 0, 0, 0},   // highlight text
        }
    },
    {
        "spruce",
        {
            {40, 40, 40, 0},   // background
            {235, 219, 178, 0}, // main text
            {96, 0, 0, 0},  // secondary text
            {104, 157, 106, 0}, // highlight background
            {40, 40, 40, 0},   // highlight text
        }
    },
    {
        "custom",
        {
            {40, 40, 40, 0},    // background
            {235, 219, 178, 0}, // main text
            {146, 138, 118, 0}, // secondary text
            {104, 157, 106, 0}, // highlight background
            {40, 40, 40, 0},    // highlight text
        }
    },
};
bool custom_chosen = false;

SDL_Color mix(SDL_Color a, SDL_Color b, int percent_of_a)
{
    SDL_Color out = {0, 0, 0, 0};
    out.r = static_cast<Uint8>((a.r * percent_of_a + b.r * (100 - percent_of_a)) / 100);
    out.g = static_cast<Uint8>((a.g * percent_of_a + b.g * (100 - percent_of_a)) / 100);
    out.b = static_cast<Uint8>((a.b * percent_of_a + b.b * (100 - percent_of_a)) / 100);
    return out;
}

int get_theme_index(const std::string &name)
{
    for (uint32_t i = 0; i < theme_defs.size(); i++)
    {
        if (theme_defs[i].first == name)
        {
            return i;
        }
    }

    return -1;
}

} // namespace

int theme_count()
{
    return static_cast<int>(theme_defs.size());
}

std::string theme_name_at(int index)
{
    return theme_defs[static_cast<size_t>(index)].first;
}

const ColorTheme& get_color_theme(const std::string &name)
{
    int i = get_theme_index(name);
    if (i < 0)
    {
        i = 0;
    }

    return theme_defs[i].second;
}

std::string get_valid_theme(const std::string &preferred)
{
    int i = get_theme_index(preferred);
    if (i < 0)
    {
        i = get_theme_index(DEFAULT_COLOR_THEME);
    }
    if (i < 0)
    {
        i = 0;
    }
    return theme_defs[i].first;
}

std::string get_prev_theme(const std::string &name)
{
    int i = get_theme_index(name);
    if (i < 0)
    {
        return theme_defs[0].first;
    }

    return theme_defs[
        (i - 1 + theme_defs.size()) % theme_defs.size()
    ].first;
}

std::string get_next_theme(const std::string &name)
{
    int i = get_theme_index(name);
    if (i < 0)
    {
        return theme_defs[0].first;
    }

    return theme_defs[
        (i + 1) % theme_defs.size()
    ].first;
}

void set_custom_theme(SDL_Color background, SDL_Color text, SDL_Color highlight)
{
    for (auto &def : theme_defs)
    {
        if (def.first == "custom")
        {
            def.second.background = background;
            def.second.main_text = text;
            def.second.secondary_text = mix(text, background, 55);
            def.second.highlight_background = highlight;
            // Black or white, whichever reads better on the highlight color.
            const int light = (highlight.r * 299 + highlight.g * 587 + highlight.b * 114) / 1000;
            def.second.highlight_text = light > 140 ? SDL_Color {0, 0, 0, 0} : SDL_Color {250, 250, 250, 0};
            custom_chosen = true;
        }
    }
}

bool custom_theme_chosen()
{
    return custom_chosen;
}

void get_custom_theme(SDL_Color &background, SDL_Color &text, SDL_Color &highlight)
{
    const ColorTheme &def = get_color_theme("custom");
    background = def.background;
    text = def.main_text;
    highlight = def.highlight_background;
}
