#ifndef TEXT_STYLE_H_
#define TEXT_STYLE_H_

#include "doc_api/doc_addr.h"
#include "doc_api/doc_token.h"

#include <SDL/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

int style_flags(bool bold, bool italic);

// Lowercase for dictionary lookup and search: ASCII, and the accented capitals of
// the Latin-1 block. The text keeps its length.
std::string lookup_lowercase(const std::string &word);

using RunFontFn = std::function<TTF_Font *(int style, float scale)>;

// Width of text[offset, offset+len), applying bold, italic, and size runs.
int measure_styled(
    const char *text,
    uint32_t offset,
    uint32_t len,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    float base_scale,
    const RunFontFn &font_for
);

std::vector<StyleSpan> clip_spans(
    const std::vector<StyleSpan> &spans,
    uint32_t shift,
    uint32_t offset,
    uint32_t len
);

struct DrawnPiece
{
    std::string text;
    int x = 0;
    // Where the piece sat before justify moved it, or -1 when it did not move.
    int src_x = -1;
    int w = 0;
    int h = 0;
    int ascent = 0;
    int style = 0;
    float scale = 1.f;
};

struct LayoutWord
{
    std::string text;
    std::string lookup;
    DocAddr begin = 0;
    DocAddr end = 0;
    int x = 0;
    int w = 0;
    int line_index = 0;
    // The last word of a line that was split with a hyphen.
    bool hyphen_end = false;
    // Where the word links to, when it is part of a link.
    std::shared_ptr<const std::string> link;
};

struct LineLayout
{
    bool justified = false;
    std::vector<DrawnPiece> pieces;
    std::vector<LayoutWord> words;
    int width = 0;
};

// Spreads the gaps between words so the line fills `width` pixels. Does nothing
// when the line is already full or is too short to stretch without looking odd.
void justify_layout(LineLayout &layout, int width);

LineLayout layout_line(
    const std::string &text,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    float base_scale,
    const RunFontFn &font_for,
    DocAddr line_address,
    uint32_t address_lead
);

#endif
