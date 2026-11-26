#ifndef HIGHLIGHTS_H_
#define HIGHLIGHTS_H_

#include "doc_api/doc_addr.h"

#include <filesystem>
#include <string>
#include <vector>

// How a highlight is drawn: a colored band behind the words, or a line under or through them.
enum class HighlightStyle {
    Marker,       // yellow
    Underline,
    Strikeout,
    MarkerGreen,
    MarkerBlue,
    MarkerPink
};

constexpr int HIGHLIGHT_COLOR_COUNT = 4;

// The colored band styles in the order of the Highlight color setting: yellow, green, blue, pink.
inline HighlightStyle marker_style(int color)
{
    switch (color)
    {
        case 1: return HighlightStyle::MarkerGreen;
        case 2: return HighlightStyle::MarkerBlue;
        case 3: return HighlightStyle::MarkerPink;
        default: return HighlightStyle::Marker;
    }
}

// The color of a band style, or -1 for underline and strikeout.
inline int marker_color_of(HighlightStyle style)
{
    switch (style)
    {
        case HighlightStyle::Marker: return 0;
        case HighlightStyle::MarkerGreen: return 1;
        case HighlightStyle::MarkerBlue: return 2;
        case HighlightStyle::MarkerPink: return 3;
        default: return -1;
    }
}

inline const char *highlight_color_name(int color)
{
    static const char *names[HIGHLIGHT_COLOR_COUNT] = {"Yellow", "Green", "Blue", "Pink"};
    return names[color < 0 ? 0 : (color >= HIGHLIGHT_COLOR_COUNT ? HIGHLIGHT_COLOR_COUNT - 1 : color)];
}

// A stretch of the book, [first, second), and the words in it as they were read.
// The text is empty for highlights saved before it was kept.
struct Highlight
{
    DocAddr first = 0;
    DocAddr second = 0;
    std::string text;
    HighlightStyle style = HighlightStyle::Marker;
    std::string note; // what the reader wrote about it, empty when nothing

    Highlight(DocAddr first, DocAddr second, std::string text = {}, HighlightStyle style = HighlightStyle::Marker)
        : first(first), second(second), text(std::move(text)), style(style)
    {
    }
};

// The letter the Markdown export and import use for a style: y, g, b, p, u or s.
char highlight_style_letter(HighlightStyle style);
HighlightStyle highlight_style_from_letter(char letter);

std::vector<Highlight> load_highlights(const std::filesystem::path &path);
void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights);
bool highlights_cover(const std::vector<Highlight> &highlights, DocAddr begin);
// Whether a highlight of this style covers the address.
// The color (0 to 3) of the band over the address, or -1 when no band covers it.
int highlight_marker_at(const std::vector<Highlight> &highlights, DocAddr begin);
bool highlights_cover_style(const std::vector<Highlight> &highlights, DocAddr begin, HighlightStyle style);
bool highlight_covers_range(const std::vector<Highlight> &highlights, DocAddr begin, DocAddr end);
// Removes the highlight over the range, or adds one carrying the given text.
void toggle_highlight(
    std::vector<Highlight> &highlights,
    DocAddr begin,
    DocAddr end,
    const std::string &text = {},
    HighlightStyle style = HighlightStyle::Marker
);

#endif