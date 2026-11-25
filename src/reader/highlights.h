#ifndef HIGHLIGHTS_H_
#define HIGHLIGHTS_H_

#include "doc_api/doc_addr.h"

#include <filesystem>
#include <string>
#include <vector>

// How a highlight is drawn: a colored band behind the words, or a line under or through them.
enum class HighlightStyle {
    Marker,
    Underline,
    Strikeout
};

// A stretch of the book, [first, second), and the words in it as they were read.
// The text is empty for highlights saved before it was kept.
struct Highlight
{
    DocAddr first = 0;
    DocAddr second = 0;
    std::string text;
    HighlightStyle style = HighlightStyle::Marker;

    Highlight(DocAddr first, DocAddr second, std::string text = {}, HighlightStyle style = HighlightStyle::Marker)
        : first(first), second(second), text(std::move(text)), style(style)
    {
    }
};

std::vector<Highlight> load_highlights(const std::filesystem::path &path);
void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights);
bool highlights_cover(const std::vector<Highlight> &highlights, DocAddr begin);
// Whether a highlight of this style covers the address.
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