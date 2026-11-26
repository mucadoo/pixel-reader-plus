#ifndef LAYOUT_OPTIONS_H_
#define LAYOUT_OPTIONS_H_

#include <string>

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

// Space left empty above and below the text, in pixels.
inline int vertical_margin_pixels(int level)
{
    static const int values[LAYOUT_LEVEL_COUNT] = {0, 8, 16, 28};
    return values[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *vertical_margin_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"None", "Small", "Medium", "Large"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

// The reading ruler: 0 off, 1 marks the line, 2 dims the others.
constexpr int RULER_MODE_COUNT = 3;

inline const char *ruler_label(int mode)
{
    static const char *labels[RULER_MODE_COUNT] = {"Off", "Line", "Focus"};
    return labels[mode < 0 ? 0 : (mode >= RULER_MODE_COUNT ? RULER_MODE_COUNT - 1 : mode)];
}

// Extra pixels after each letter.
inline int letter_spacing_pixels(int level)
{
    return level < 0 ? 0 : (level > LAYOUT_LEVEL_COUNT - 1 ? LAYOUT_LEVEL_COUNT - 1 : level);
}

inline const char *letter_spacing_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"Normal", "Slight", "Wide", "Wider"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *grid_size_label(int level)
{
    static const char *labels[3] = {"Large", "Medium", "Small"};
    return labels[level < 0 ? 0 : (level > 2 ? 2 : level)];
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

// Extra pixels added to every space between words.
inline int word_spacing_pixels(int level)
{
    static const int values[LAYOUT_LEVEL_COUNT] = {-1, 0, 2, 4};
    return values[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *word_spacing_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"Tight", "Normal", "Wide", "Wider"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

// Minutes of quiet before the cover screen, per step of the Cover screen setting. Step 0 is off.
inline int cover_sleep_minutes(int step)
{
    static const int values[4] = {0, 2, 5, 10};
    return values[step < 0 ? 0 : (step > 3 ? 3 : step)];
}

inline std::string cover_sleep_label(int step)
{
    return step <= 0 ? std::string("Off") : std::to_string(cover_sleep_minutes(step)) + " min";
}

// First-line indent of a paragraph, in pixels.
inline int indent_pixels(int level)
{
    static const int values[LAYOUT_LEVEL_COUNT] = {0, 14, 24, 36};
    return values[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

inline const char *indent_label(int level)
{
    static const char *labels[LAYOUT_LEVEL_COUNT] = {"None", "Small", "Medium", "Large"};
    return labels[level < 0 ? 0 : (level >= LAYOUT_LEVEL_COUNT ? LAYOUT_LEVEL_COUNT - 1 : level)];
}

// Minutes of no key presses before the reader saves and quits. 0 is never.
constexpr int AUTO_EXIT_STEP_COUNT = 6;

inline int auto_exit_minutes_for_step(int step)
{
    static const int values[AUTO_EXIT_STEP_COUNT] = {0, 5, 10, 15, 30, 60};
    return values[step < 0 ? 0 : (step >= AUTO_EXIT_STEP_COUNT ? AUTO_EXIT_STEP_COUNT - 1 : step)];
}

inline int auto_exit_step_for_minutes(int minutes)
{
    for (int step = 0; step < AUTO_EXIT_STEP_COUNT; ++step)
    {
        if (auto_exit_minutes_for_step(step) == minutes)
        {
            return step;
        }
    }
    return 0;
}

inline std::string auto_exit_label(int minutes)
{
    return minutes <= 0 ? std::string("Off") : std::to_string(minutes) + " min";
}

constexpr int DEFAULT_MARGIN_LEVEL = 0;
constexpr int DEFAULT_SPACING_LEVEL = 1;
constexpr int DEFAULT_INDENT_LEVEL = 0;

#endif