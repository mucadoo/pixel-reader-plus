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

// Extra pixels added to every space between words (negative to tighten). Applies to later layouts.
void set_word_spacing(int pixels);

// Other fonts to draw letters with when the font has none for them (symbols, Greek, Cyrillic, CJK).
// The first of them that has the letter is used. Applies to later layouts and drawing.
void set_font_fallbacks(TTF_Font *primary, std::vector<TTF_Font *> fallbacks);

// The width of the text in pixels, with fallback fonts for the letters the font lacks. Letter spacing is not added.
int measure_text(TTF_Font *font, const char *text, uint32_t len);

// Draw ff, fi, fl, ffi and ffl as one letter when the font has it. Applies to later layouts and drawing.
void set_ligatures(bool on);

// Extra pixels added after every letter (0 for none). Applies to later layouts and drawing.
void set_letter_spacing(int pixels);

// True when the word or letter spacing is not the font's own, so a line cannot be drawn as one plain string.
bool spacing_is_custom();

// TTF_RenderUTF8_Shaded, with the letter spacing applied. The caller frees the surface.
SDL_Surface *render_text_shaded(TTF_Font *font, const char *text, SDL_Color fg, SDL_Color bg);

// Forget the letters kept for drawing, for example after the font changed.
void clear_glyph_cache();

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
    // The last word of the last line of a paragraph.
    bool paragraph_end = false;
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
