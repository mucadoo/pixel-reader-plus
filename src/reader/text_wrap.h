#ifndef TEXT_WRAP_H_
#define TEXT_WRAP_H_

#include <functional>
#include <string>
#include <vector>

// With this on a line is not broken where French and Portuguese typesetting keeps words together: before
// : ; ! ? % and a closing guillemet, after an opening one, and between a number and its unit.
void set_keep_punctuation(bool on);

void wrap_lines(
    const char *str,
    std::function<bool(const char *, uint32_t)> fits_on_line,
    std::function<void(const char *, uint32_t)> on_next_line,
    unsigned int max_line_search_chars = 1024
);

// Where a word may be split, as byte offsets inside the word, smallest first.
using HyphenPointsFn = std::function<std::vector<uint32_t>(const char *word, uint32_t len)>;

// Like wrap_lines, but a word that does not fit may be split. The first piece
// goes on the line and the report says it ends in a hyphen, which is not part of
// the text. fits_with_hyphen tells whether the piece and a hyphen fit.
void wrap_lines_hyphenated(
    const char *str,
    std::function<bool(const char *, uint32_t)> fits_on_line,
    std::function<bool(const char *, uint32_t)> fits_with_hyphen,
    HyphenPointsFn hyphen_points,
    std::function<void(const char *, uint32_t, bool hyphenated)> on_next_line,
    unsigned int max_line_search_chars = 1024
);

#endif
