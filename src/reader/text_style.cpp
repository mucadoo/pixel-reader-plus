#include "./text_style.h"

#include "util/str_utils.h"
#include "util/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

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

int measure_plain(TTF_Font *font, const char *s, uint32_t len)
{
    if (!font || len == 0)
    {
        return 0;
    }
    char *mut = const_cast<char *>(s);
    char saved = mut[len];
    mut[len] = 0;
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(font, mut, &w, &h);
    mut[len] = saved;
    return w;
}

std::string lookup_key(const std::string &word)
{
    std::string lower = to_lower(word);
    auto ok = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-';
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
        width += measure_plain(font_for(run.flags, run.scale), p, static_cast<uint32_t>(q - p));
        p = q;
    }
    return width;
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
