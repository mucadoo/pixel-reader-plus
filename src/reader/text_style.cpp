#include "./text_style.h"

#include "util/str_utils.h"
#include "util/utf8.h"

#include <algorithm>

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

int style_at(uint32_t offset, const std::vector<StyleSpan> &spans, bool base_bold, bool base_italic)
{
    bool bold = base_bold;
    bool italic = base_italic;
    for (const StyleSpan &span : spans)
    {
        if (offset >= span.start && offset < span.end)
        {
            bold = bold || span.bold;
            italic = italic || span.italic;
            break;
        }
    }
    return style_flags(bold, italic);
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
    const std::function<TTF_Font *(int style)> &font_for
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
        int style = style_at(static_cast<uint32_t>(p - text), spans, base_bold, base_italic);
        const char *q = p;
        while (q < end && is_whitespace(*q) == ws && style_at(static_cast<uint32_t>(q - text), spans, base_bold, base_italic) == style)
        {
            const char *next = utf8_step(q);
            if (next == q)
            {
                break;
            }
            q = next;
        }
        width += measure_plain(font_for(style), p, static_cast<uint32_t>(q - p));
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
    const std::function<TTF_Font *(int style)> &font_for,
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
        int style = style_at(static_cast<uint32_t>(p - base), spans, base_bold, base_italic);
        const char *q = p;
        while (q < end && is_whitespace(*q) == ws && style_at(static_cast<uint32_t>(q - base), spans, base_bold, base_italic) == style)
        {
            const char *next = utf8_step(q);
            if (next == q)
            {
                break;
            }
            q = next;
        }

        int piece_w = measure_plain(font_for(style), p, static_cast<uint32_t>(q - p));
        DrawnPiece piece;
        piece.text.assign(p, q);
        piece.x = x;
        piece.w = piece_w;
        piece.style = style;
        layout.pieces.push_back(piece);

        if (ws)
        {
            finish_word();
        }
        else
        {
            DocAddr piece_begin = addr;
            bool any_real = false;
            for (const char *c = p; c < q; )
            {
                if (lead > 0)
                {
                    --lead;
                }
                else
                {
                    if (!any_real)
                    {
                        piece_begin = addr;
                        any_real = true;
                    }
                    ++addr;
                }
                const char *next = utf8_step(c);
                if (next == c)
                {
                    break;
                }
                c = next;
            }

            if (any_real)
            {
                if (!in_word)
                {
                    word = LayoutWord();
                    word.x = x;
                    word.begin = piece_begin;
                    in_word = true;
                }
                word.text.append(p, q);
                word.end = addr;
            }
        }

        x += piece_w;
        p = q;
    }

    finish_word();
    layout.width = x;
    return layout;
}
