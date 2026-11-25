#ifndef HIGHLIGHTS_H_
#define HIGHLIGHTS_H_

#include "doc_api/doc_addr.h"

#include <filesystem>
#include <string>
#include <vector>

// A stretch of the book, [first, second), and the words in it as they were read.
// The text is empty for highlights saved before it was kept.
struct Highlight
{
    DocAddr first = 0;
    DocAddr second = 0;
    std::string text;

    Highlight(DocAddr first, DocAddr second, std::string text = {})
        : first(first), second(second), text(std::move(text))
    {
    }
};

std::vector<Highlight> load_highlights(const std::filesystem::path &path);
void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights);
bool highlights_cover(const std::vector<Highlight> &highlights, DocAddr begin);
bool highlight_covers_range(const std::vector<Highlight> &highlights, DocAddr begin, DocAddr end);
// Removes the highlight over the range, or adds one carrying the given text.
void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end, const std::string &text = {});

#endif