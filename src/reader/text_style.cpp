#include "./text_style.h"

#include "util/str_utils.h"
#include "util/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>

int style_flags(bool bold, bool italic)
{
    int flags = TTF_STYLE_NORMAL;
    if (bold)
    {
        flags |= TTF_STYLE_BOLD;
    }
    if (italic)
    {
        flags |= TTF_STYLE_ITALIC;
    }
    return flags;
}

namespace
{

struct RunStyle
{
    int flags = 0;
    float scale = 1.f;

    bool operator==(const RunStyle &other) const
    {
        return flags == other.flags && std::fabs(scale - other.scale) < 0.02f;
    }
};

RunStyle style_at(
    uint32_t offset,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    float base_scale
)
{
    bool bold = base_bold;
    bool italic = base_italic;
    float scale = base_scale > 0.2f ? base_scale : 1.f;
    for (const StyleSpan &span : spans)
    {
        if (offset >= span.start && offset < span.end)
        {
            bold = bold || span.bold;
            italic = italic || span.italic;
            if (span.scale > 0.2f)
            {
                scale = span.scale;
            }
            break;
        }
    }
    return {style_flags(bold, italic), scale};
}

uint32_t utf8_codepoint(const char *s, const char *end)
{
    const unsigned char *u = reinterpret_cast<const unsigned char *>(s);
    size_t n = static_cast<size_t>(end - s);
    if (n == 0)
    {
        return 0;
    }
    if (u[0] < 0x80)
    {
        return u[0];
    }
    if (n >= 2 && (u[0] & 0xE0) == 0xC0)
    {
        return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
    }
    if (n >= 3 && (u[0] & 0xF0) == 0xE0)
    {
        return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
    }
    if (n >= 4 && (u[0] & 0xF8) == 0xF0)
    {
        return ((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12) | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F);
    }
    return u[0];
}

// Letters and digits stay in the word. Commas, periods, quotes, and dashes do not.
bool is_word_cp(uint32_t cp)
{
    if (cp < 128)
    {
        bool digit = cp >= '0' && cp <= '9';
        bool upper = cp >= 'A' && cp <= 'Z';
        bool lower = cp >= 'a' && cp <= 'z';
        return digit || upper || lower || cp == '\'' || cp == '-';
    }
    if (cp == 0x00AB || cp == 0x00BB)
    {
        return false;
    }
    if (cp >= 0x2010 && cp <= 0x2027)
    {
        return false;
    }
    if (cp >= 0x2000 && cp <= 0x206F)
    {
        return false;
    }
    if (cp >= 0x3000 && cp <= 0x303F)
    {
        return false;
    }
    return true;
}

int g_letter_spacing = 0;

size_t count_codepoints(const char *s, uint32_t len)
{
    size_t count = 0;
    for (uint32_t i = 0; i < len; ++i)
    {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
        {
            ++count;
        }
    }
    return count;
}

// Fonts to draw a letter with when the font of the text has none for it, in order of preference.
std::map<TTF_Font *, std::vector<TTF_Font *>> g_fallbacks;

uint32_t decode_codepoint(const char *p, const char *end, const char **next)
{
    const unsigned char c = static_cast<unsigned char>(*p);
    uint32_t cp = c;
    int more = 0;
    if (c >= 0xF0) { cp = c & 0x07; more = 3; }
    else if (c >= 0xE0) { cp = c & 0x0F; more = 2; }
    else if (c >= 0xC0) { cp = c & 0x1F; more = 1; }
    ++p;
    while (more-- > 0 && p < end && (static_cast<unsigned char>(*p) & 0xC0) == 0x80)
    {
        cp = (cp << 6) | (static_cast<unsigned char>(*p) & 0x3F);
        ++p;
    }
    *next = p;
    return cp;
}

// Whether the font draws something of its own for the letter. A font without it gives its
// "missing letter" shape, which is what a letter that no font has looks like too.
bool font_has_glyph(TTF_Font *font, uint32_t cp)
{
    if (cp < 0x80)
    {
        return true;
    }
    if (cp > 0xFFFF)
    {
        return true; // the font library reads only the first 65 536 letters
    }
    static std::map<std::pair<TTF_Font *, uint32_t>, bool> known;
    const auto key = std::make_pair(font, cp);
    const auto found = known.find(key);
    if (found != known.end())
    {
        return found->second;
    }
    if (known.size() > 20000)
    {
        known.clear();
    }
    int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
    int bminx = 0, bmaxx = 0, bminy = 0, bmaxy = 0, badvance = 0;
    const bool ok = TTF_GlyphMetrics(font, static_cast<Uint16>(cp), &minx, &maxx, &miny, &maxy, &advance) == 0;
    const bool bad_ok = TTF_GlyphMetrics(font, 0xFFFE, &bminx, &bmaxx, &bminy, &bmaxy, &badvance) == 0;
    const bool has = ok && !(bad_ok && minx == bminx && maxx == bmaxx && miny == bminy && maxy == bmaxy && advance == badvance);
    known[key] = has;
    return has;
}

// The font to draw the letter with: the text font, or the first fallback that has it.
TTF_Font *font_for_codepoint(TTF_Font *font, uint32_t cp)
{
    if (cp < 0x80)
    {
        return font;
    }
    const auto it = g_fallbacks.find(font);
    if (it == g_fallbacks.end() || font_has_glyph(font, cp))
    {
        return font;
    }
    for (TTF_Font *other : it->second)
    {
        if (font_has_glyph(other, cp))
        {
            return other;
        }
    }
    return font;
}

// True when some letter of the text needs another font.
bool needs_fallback(TTF_Font *font, const char *text, uint32_t len)
{
    if (g_fallbacks.find(font) == g_fallbacks.end())
    {
        return false;
    }
    const char *end = text + len;
    for (const char *p = text; p < end;)
    {
        const char *next = p;
        const uint32_t cp = decode_codepoint(p, end, &next);
        if (cp >= 0x80 && font_for_codepoint(font, cp) != font)
        {
            return true;
        }
        p = next;
    }
    return false;
}

// The width of the text, each letter in the font that draws it. s need not end at len.
int size_plain(TTF_Font *font, const char *s, uint32_t len)
{
    int w = 0;
    int h = 0;
    if (g_fallbacks.find(font) == g_fallbacks.end() || !needs_fallback(font, s, len))
    {
        if (s[len] == 0)
        {
            TTF_SizeUTF8(font, s, &w, &h);
            return w;
        }
        char *mut = const_cast<char *>(s);
        const char saved = mut[len];
        mut[len] = 0;
        TTF_SizeUTF8(font, mut, &w, &h);
        mut[len] = saved;
        return w;
    }
    const std::string text(s, len);
    const char *end = text.data() + len;
    const char *run_start = text.data();
    TTF_Font *run_font = nullptr;
    int total = 0;
    const auto flush = [&](const char *upto) {
        if (run_font && upto > run_start)
        {
            const std::string piece(run_start, upto);
            int pw = 0, ph = 0;
            TTF_SizeUTF8(run_font, piece.c_str(), &pw, &ph);
            total += pw;
        }
    };
    for (const char *p = text.data(); p < end;)
    {
        const char *next = p;
        const uint32_t cp = decode_codepoint(p, end, &next);
        TTF_Font *wanted = font_for_codepoint(font, cp);
        if (wanted != run_font)
        {
            flush(p);
            run_start = p;
            run_font = wanted;
        }
        p = next;
    }
    flush(end);
    return total;
}

int measure_plain(TTF_Font *font, const char *s, uint32_t len)
{
    if (!font || len == 0)
    {
        return 0;
    }
    const int w0 = size_plain(font, s, len);
    int w = w0;
    if (g_letter_spacing > 0)
    {
        w += static_cast<int>(count_codepoints(s, len)) * g_letter_spacing;
    }
    return w;
}

int g_word_spacing = 0;

// The extra width the word spacing setting gives a run of whitespace.
int spacing_for(const char *s, const char *e)
{
    if (g_word_spacing == 0)
    {
        return 0;
    }
    int spaces = 0;
    for (; s < e; ++s)
    {
        spaces += *s == ' ' ? 1 : 0;
    }
    return spaces * g_word_spacing;
}

} // namespace

void set_word_spacing(int pixels)
{
    g_word_spacing = pixels;
}

void set_font_fallbacks(TTF_Font *primary, std::vector<TTF_Font *> fallbacks)
{
    if (!primary)
    {
        return;
    }
    if (fallbacks.empty())
    {
        g_fallbacks.erase(primary);
    }
    else
    {
        g_fallbacks[primary] = std::move(fallbacks);
    }
}

int measure_text(TTF_Font *font, const char *text, uint32_t len)
{
    return font ? size_plain(font, text, len) : 0;
}

void set_letter_spacing(int pixels)
{
    g_letter_spacing = std::max(0, pixels);
}

bool spacing_is_custom()
{
    return g_word_spacing != 0 || g_letter_spacing != 0;
}

namespace
{

struct GlyphKey
{
    TTF_Font *font;
    uint32_t glyph; // up to four bytes of UTF-8
    uint32_t fg;
    uint32_t bg;

    bool operator<(const GlyphKey &o) const
    {
        if (font != o.font) return font < o.font;
        if (glyph != o.glyph) return glyph < o.glyph;
        if (fg != o.fg) return fg < o.fg;
        return bg < o.bg;
    }
};

std::map<GlyphKey, SDL_Surface *> g_glyphs;
constexpr size_t MAX_GLYPHS = 3000;

uint32_t pack_color(SDL_Color c)
{
    return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
}

} // namespace

void clear_glyph_cache()
{
    for (auto &entry : g_glyphs)
    {
        if (entry.second)
        {
            SDL_FreeSurface(entry.second);
        }
    }
    g_glyphs.clear();
}

SDL_Surface *render_text_shaded(TTF_Font *font, const char *text, SDL_Color fg, SDL_Color bg)
{
    if (!font || !text || !*text || (g_letter_spacing <= 0 && !needs_fallback(font, text, static_cast<uint32_t>(std::strlen(text)))))
    {
        return TTF_RenderUTF8_Shaded(font, text ? text : "", fg, bg);
    }
    if (g_glyphs.size() > MAX_GLYPHS)
    {
        clear_glyph_cache();
    }

    struct Placed
    {
        SDL_Surface *glyph;
        int x;
        int ascent;
    };
    std::vector<Placed> placed;
    int x = 0;
    SDL_Surface *palette_from = nullptr;
    for (const char *p = text; *p;)
    {
        const char *next = utf8_step(p);
        if (next == p)
        {
            break;
        }
        std::string one(p, next);
        const char *after = p;
        TTF_Font *glyph_font = font_for_codepoint(font, decode_codepoint(p, p + (next - p), &after));
        // Where this letter starts is the width of all before it, so kerning agrees with the layout.
        int w = 0;
        if (p != text)
        {
            w = size_plain(font, text, static_cast<uint32_t>(p - text));
        }
        x = w + static_cast<int>(placed.size()) * g_letter_spacing;
        SDL_Surface *glyph = nullptr;
        if (one != " " && one != "\t")
        {
            uint32_t packed = 0;
            for (unsigned char byte : one)
            {
                packed = (packed << 8) | byte;
            }
            const GlyphKey key = {glyph_font, packed, pack_color(fg), pack_color(bg)};
            auto found = g_glyphs.find(key);
            if (found == g_glyphs.end())
            {
                found = g_glyphs.emplace(key, TTF_RenderUTF8_Shaded(glyph_font, one.c_str(), fg, bg)).first;
            }
            glyph = found->second;
            if (glyph && !palette_from)
            {
                palette_from = glyph;
            }
        }
        placed.push_back({glyph, x, TTF_FontAscent(glyph_font)});
        p = next;
    }
    int max_ascent = 0;
    int max_descent = 0;
    for (const Placed &item : placed)
    {
        if (item.glyph)
        {
            max_ascent = std::max(max_ascent, item.ascent);
            max_descent = std::max(max_descent, item.glyph->h - item.ascent);
        }
    }
    x = size_plain(font, text, static_cast<uint32_t>(std::strlen(text))) + static_cast<int>(placed.size()) * g_letter_spacing;
    if (!palette_from)
    {
        return TTF_RenderUTF8_Shaded(font, " ", fg, bg); // nothing to see but spaces
    }
    SDL_Surface *out = SDL_CreateRGBSurface(SDL_SWSURFACE, std::max(1, x), std::max(palette_from->h, max_ascent + max_descent), 8, 0, 0, 0, 0);
    if (!out)
    {
        return nullptr;
    }
    SDL_SetColors(out, palette_from->format->palette->colors, 0, palette_from->format->palette->ncolors);
    SDL_FillRect(out, nullptr, 0);
    for (const Placed &item : placed)
    {
        if (item.glyph)
        {
            SDL_Rect where = {static_cast<Sint16>(item.x), static_cast<Sint16>(max_ascent - item.ascent), 0, 0};
            SDL_BlitSurface(item.glyph, nullptr, out, &where);
        }
    }
    return out;
}

// Lowercase for dictionary lookup: ASCII, and the accented capitals of the
// Latin-1 block (such as the E in Ecole) that French and Portuguese use.
std::string lookup_lowercase(const std::string &word)
{
    std::string lower = to_lower(word);
    for (size_t i = 0; i + 1 < lower.size(); ++i)
    {
        unsigned char lead = static_cast<unsigned char>(lower[i]);
        unsigned char next = static_cast<unsigned char>(lower[i + 1]);
        if (lead == 0xC3 && next >= 0x80 && next <= 0x9E && next != 0x97)
        {
            lower[i + 1] = static_cast<char>(next + 0x20);
            ++i;
        }
        else if (lead == 0xC5 && next == 0x92)
        {
            lower[i + 1] = static_cast<char>(0x93); // OE ligature
            ++i;
        }
        else if (lead >= 0xC2)
        {
            ++i; // skip the rest of this letter
        }
    }
    return lower;
}

namespace
{

std::string lookup_key(const std::string &word)
{
    std::string lower = lookup_lowercase(word);
    // A curly apostrophe is looked up as a straight one.
    for (size_t at = lower.find("\xE2\x80\x99"); at != std::string::npos; at = lower.find("\xE2\x80\x99", at + 1))
    {
        lower.replace(at, 3, "'");
    }
    for (size_t at = lower.find("\xE2\x80\x98"); at != std::string::npos; at = lower.find("\xE2\x80\x98", at + 1))
    {
        lower.replace(at, 3, "'");
    }
    // Bytes of letters outside ASCII belong to the word.
    auto ok = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-' || c >= 0x80;
    };
    size_t begin = 0;
    size_t end = lower.size();
    while (begin < end && !ok(static_cast<unsigned char>(lower[begin])))
    {
        ++begin;
    }
    while (end > begin && !ok(static_cast<unsigned char>(lower[end - 1])))
    {
        --end;
    }
    while (begin < end && (lower[begin] == '\'' || lower[begin] == '-'))
    {
        ++begin;
    }
    while (end > begin && (lower[end - 1] == '\'' || lower[end - 1] == '-'))
    {
        --end;
    }
    return lower.substr(begin, end - begin);
}

} // namespace

int measure_styled(
    const char *text,
    uint32_t offset,
    uint32_t len,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    float base_scale,
    const RunFontFn &font_for
)
{
    if (!text || len == 0)
    {
        return 0;
    }

    int width = 0;
    const char *p = text + offset;
    const char *end = p + len;
    while (p < end)
    {
        bool ws = is_whitespace(*p);
        RunStyle run = style_at(static_cast<uint32_t>(p - text), spans, base_bold, base_italic, base_scale);
        const char *q = p;
        while (q < end && is_whitespace(*q) == ws && style_at(static_cast<uint32_t>(q - text), spans, base_bold, base_italic, base_scale) == run)
        {
            const char *next = utf8_step(q);
            if (next == q)
            {
                break;
            }
            q = next;
        }
        int run_w = measure_plain(font_for(run.flags, run.scale), p, static_cast<uint32_t>(q - p));
        if (ws)
        {
            run_w = std::max(0, run_w + spacing_for(p, q));
        }
        width += run_w;
        p = q;
    }
    return width;
}

// The link the text at this byte offset belongs to, if any.
static std::shared_ptr<const std::string> link_at(uint32_t offset, const std::vector<StyleSpan> &spans)
{
    for (const StyleSpan &span : spans)
    {
        if (span.href && offset >= span.start && offset < span.end)
        {
            return span.href;
        }
    }
    return nullptr;
}

std::vector<StyleSpan> clip_spans(
    const std::vector<StyleSpan> &spans,
    uint32_t shift,
    uint32_t offset,
    uint32_t len
)
{
    std::vector<StyleSpan> clipped;
    uint32_t end = offset + len;
    for (StyleSpan span : spans)
    {
        uint32_t start = span.start + shift;
        uint32_t stop = span.end + shift;
        if (stop <= offset || start >= end)
        {
            continue;
        }
        span.start = std::max(start, offset) - offset;
        span.end = std::min(stop, end) - offset;
        if (span.end > span.start)
        {
            clipped.push_back(span);
        }
    }
    return clipped;
}

LineLayout layout_line(
    const std::string &text,
    const std::vector<StyleSpan> &spans,
    bool base_bold,
    bool base_italic,
    float base_scale,
    const RunFontFn &font_for,
    DocAddr line_address,
    uint32_t address_lead
)
{
    LineLayout layout;
    const char *base = text.c_str();
    const char *p = base;
    const char *end = base + text.size();
    DocAddr addr = line_address;
    uint32_t lead = address_lead;
    int x = 0;

    LayoutWord word;
    bool in_word = false;

    auto finish_word = [&]() {
        if (!in_word)
        {
            return;
        }
        word.w = x - word.x;
        word.lookup = lookup_key(word.text);
        if (!word.lookup.empty() && word.end > word.begin && word.w > 0)
        {
            layout.words.push_back(word);
        }
        in_word = false;
        word = LayoutWord();
    };

    while (p < end)
    {
        bool ws = is_whitespace(*p);
        RunStyle run = style_at(static_cast<uint32_t>(p - base), spans, base_bold, base_italic, base_scale);
        const char *q = p;
        while (q < end && is_whitespace(*q) == ws && style_at(static_cast<uint32_t>(q - base), spans, base_bold, base_italic, base_scale) == run)
        {
            const char *next = utf8_step(q);
            if (next == q)
            {
                break;
            }
            q = next;
        }

        TTF_Font *font = font_for(run.flags, run.scale);
        int piece_w = measure_plain(font, p, static_cast<uint32_t>(q - p));
        if (ws)
        {
            piece_w = std::max(0, piece_w + spacing_for(p, q));
        }
        DrawnPiece piece;
        piece.text.assign(p, q);
        piece.x = x;
        piece.w = piece_w;
        piece.style = run.flags;
        piece.scale = run.scale;
        if (font)
        {
            piece.ascent = TTF_FontAscent(font);
            piece.h = TTF_FontHeight(font);
        }
        layout.pieces.push_back(piece);

        if (ws)
        {
            finish_word();
            x += piece_w;
        }
        else
        {
            int piece_origin = x;
            int consumed = 0;
            for (const char *c = p; c < q; )
            {
                const char *next = utf8_step(c);
                if (next == c)
                {
                    break;
                }
                bool skipped_lead = false;
                DocAddr cp_addr = addr;
                if (lead > 0)
                {
                    --lead;
                    skipped_lead = true;
                }
                else
                {
                    ++addr;
                }
                int through = measure_plain(font, p, static_cast<uint32_t>(next - p));
                int cp_w = through - consumed;
                int cp_x = piece_origin + consumed;
                consumed = through;
                uint32_t cp = utf8_codepoint(c, next);
                if (!skipped_lead && is_word_cp(cp))
                {
                    if (!in_word)
                    {
                        word = LayoutWord();
                        word.x = cp_x;
                        word.begin = cp_addr;
                        word.link = link_at(static_cast<uint32_t>(c - base), spans);
                        in_word = true;
                    }
                    word.text.append(c, next);
                    word.end = addr;
                }
                else
                {
                    finish_word();
                }
                x = cp_x + cp_w;
                c = next;
            }
            x = piece_origin + piece_w;
        }
        p = q;
    }

    finish_word();
    layout.width = x;
    return layout;
}

namespace
{

bool piece_is_blank(const DrawnPiece &piece)
{
    for (char c : piece.text)
    {
        if (!is_whitespace(c))
        {
            return false;
        }
    }
    return true;
}

} // namespace

void justify_layout(LineLayout &layout, int width)
{
    const int extra = width - layout.width;
    if (extra <= 0 || layout.justified)
    {
        return;
    }
    // A line that falls well short of full is better left alone.
    if (extra * 4 > layout.width)
    {
        return;
    }

    int first = -1;
    int last = -1;
    for (int i = 0; i < static_cast<int>(layout.pieces.size()); ++i)
    {
        if (!piece_is_blank(layout.pieces[i]))
        {
            if (first < 0)
            {
                first = i;
            }
            last = i;
        }
    }
    if (first < 0)
    {
        return;
    }

    int gaps = 0;
    for (int i = first + 1; i < last; ++i)
    {
        if (piece_is_blank(layout.pieces[i]))
        {
            ++gaps;
        }
    }
    if (gaps == 0)
    {
        return;
    }

    const int share = extra / gaps;
    const int remainder = extra % gaps;
    std::vector<std::pair<int, int>> shift_after; // (x where a gap ended, total shift from there on)
    int total = 0;
    int gap_index = 0;
    for (int i = 0; i < static_cast<int>(layout.pieces.size()); ++i)
    {
        DrawnPiece &piece = layout.pieces[i];
        const int original_x = piece.x;
        const int original_w = piece.w;
        piece.src_x = original_x;
        piece.x = original_x + total;
        if (i > first && i < last && piece_is_blank(piece))
        {
            const int add = share + (gap_index < remainder ? 1 : 0);
            ++gap_index;
            piece.w = original_w + add;
            total += add;
            shift_after.emplace_back(original_x + original_w, total);
        }
    }

    for (LayoutWord &word : layout.words)
    {
        int shift = 0;
        for (const auto &entry : shift_after)
        {
            if (entry.first <= word.x)
            {
                shift = entry.second;
            }
        }
        word.x += shift;
    }

    layout.width += total;
    layout.justified = true;
}