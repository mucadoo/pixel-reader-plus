#ifndef TEXT_STYLE_H_
#define TEXT_STYLE_H_

#include "doc_api/doc_addr.h"
#include "doc_api/doc_token.h"

#include <SDL/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

int style_flags(bool bold, bool italic);

// Width of text[offset, offset+len), applying bold and italic runs.
int measure_styled(
    const char *text,
    uint32_t offset,
    uint32_t len,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    const std::function<TTF_Font *(int style)> &font_for
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
    int w = 0;
    int style = 0;
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
};

struct LineLayout
{
    std::vector<DrawnPiece> pieces;
    std::vector<LayoutWord> words;
    int width = 0;
};

LineLayout layout_line(
    const std::string &text,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    const std::function<TTF_Font *(int style)> &font_for,
    DocAddr line_address,
    uint32_t address_lead
);

#endif
