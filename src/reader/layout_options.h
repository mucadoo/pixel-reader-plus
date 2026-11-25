#ifndef LAYOUT_OPTIONS_H_
#define LAYOUT_OPTIONS_H_

// The steps of the margin and line spacing settings. Level 0 is the first listed.

constexpr int LAYOUT_LEVEL_COUNT = 4;

// Space left empty at each side of the text, in pixels.
inline int margin_pixels(int level)
{
    static const int values[LAYOUT_LEVEL_COUNT] = {4, 16, 28, 44};
    return values[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *margin_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"Narrow", "Medium", "Wide", "Extra wide"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

// Pixels added to the height of every line, on top of the font's own.
inline int spacing_pixels(int level)
{
    static const int values[LAYOUT_LEVEL_COUNT] = {-2, 0, 4, 8};
    return values[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *spacing_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"Tight", "Normal", "Relaxed", "Loose"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

constexpr int DEFAULT_MARGIN_LEVEL = 0;
constexpr int DEFAULT_SPACING_LEVEL = 1;

#endif