#include "./markup.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>

namespace markup
{

namespace
{

void append_utf8(std::string &out, uint32_t cp)
{
    if (cp < 0x80)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool valid_utf8(const std::string &s)
{
    size_t i = 0;
    while (i < s.size())
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t extra = 0;
        if (c < 0x80) extra = 0;
        else if (c >= 0xC2 && c <= 0xDF) extra = 1;
        else if (c >= 0xE0 && c <= 0xEF) extra = 2;
        else if (c >= 0xF0 && c <= 0xF4) extra = 3;
        else return false;
        for (size_t k = 1; k <= extra; ++k)
        {
            if (i + k >= s.size() || (static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += extra + 1;
    }
    return true;
}

// Windows-1252 for the bytes 0x80 to 0x9F, which Latin-1 leaves as controls.
const uint16_t CP1252[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

std::string lower(std::string s)
{
    for (auto &c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Puts an empty line between blocks that are not tightly joined, such as list items.
std::vector<Block> with_gaps(const std::vector<Block> &in, const std::vector<bool> &tight_before)
{
    std::vector<Block> out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (!out.empty())
        {
            const bool both_items = out.back().kind == Block::Kind::ListItem && in[i].kind == Block::Kind::ListItem;
            const bool tight = i < tight_before.size() && tight_before[i];
            if (!both_items && !tight)
            {
                Block gap;
                gap.kind = Block::Kind::Blank;
                out.push_back(gap);
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

// ---------------------------------------------------------------- Markdown

// Inline markers: **bold**, *italic*, _italic_, `code`, [text](link). Escapes with a backslash.
void inline_markdown(const std::string &in, std::string &out, std::vector<StyleSpan> &spans)
{
    bool bold = false;
    bool italic = false;
    size_t segment_start = 0;
    auto close_segment = [&]() {
        if ((bold || italic) && out.size() > segment_start)
        {
            StyleSpan span;
            span.start = static_cast<uint32_t>(segment_start);
            span.end = static_cast<uint32_t>(out.size());
            span.bold = bold;
            span.italic = italic;
            spans.push_back(span);
        }
        segment_start = out.size();
    };
    auto word_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; };

    for (size_t i = 0; i < in.size(); ++i)
    {
        const char c = in[i];
        if (c == '\\' && i + 1 < in.size() && std::ispunct(static_cast<unsigned char>(in[i + 1])))
        {
            out.push_back(in[i + 1]);
            ++i;
            continue;
        }
        if (c == '`')
        {
            const size_t end = in.find('`', i + 1);
            if (end != std::string::npos)
            {
                out.append(in, i + 1, end - i - 1);
                i = end;
                continue;
            }
        }
        if (c == '!' && i + 1 < in.size() && in[i + 1] == '[')
        {
            // An image: show its description
            const size_t close = in.find(']', i + 2);
            if (close != std::string::npos && close + 1 < in.size() && in[close + 1] == '(')
            {
                const size_t paren = in.find(')', close + 2);
                if (paren != std::string::npos)
                {
                    out.append(in, i + 2, close - i - 2);
                    i = paren;
                    continue;
                }
            }
        }
        if (c == '[')
        {
            const size_t close = in.find(']', i + 1);
            if (close != std::string::npos && close + 1 < in.size() && in[close + 1] == '(')
            {
                const size_t paren = in.find(')', close + 2);
                if (paren != std::string::npos)
                {
                    std::string label;
                    std::vector<StyleSpan> inner;
                    inline_markdown(in.substr(i + 1, close - i - 1), label, inner);
                    close_segment();
                    const size_t base = out.size();
                    out += label;
                    for (StyleSpan span : inner)
                    {
                        span.start += static_cast<uint32_t>(base);
                        span.end += static_cast<uint32_t>(base);
                        spans.push_back(span);
                    }
                    segment_start = out.size();
                    i = paren;
                    continue;
                }
            }
        }
        if (c == '~' && i + 1 < in.size() && in[i + 1] == '~')
        {
            ++i; // strikethrough markers are dropped
            continue;
        }
        if ((c == '*' || c == '_') && i + 1 < in.size())
        {
            const bool double_marker = in[i + 1] == c;
            const size_t width = double_marker ? 2 : 1;
            const bool is_open = double_marker ? !bold : !italic;
            const size_t after = i + width;
            if (is_open)
            {
                // an opening marker needs a closing one later, and text right after it
                const std::string marker(width, c);
                const bool text_follows = after < in.size() && !is_space(in[after]);
                const bool boundary = c == '*' || i == 0 || !word_char(in[i - 1]);
                size_t closing = std::string::npos;
                if (text_follows && boundary)
                {
                    for (size_t k = after; k + width <= in.size(); ++k)
                    {
                        if (in.compare(k, width, marker) == 0 && !is_space(in[k - 1])
                            && (c == '*' || k + width >= in.size() || !word_char(in[k + width]))
                            && (width == 2 || (k + 1 >= in.size() || in[k + 1] != c)))
                        {
                            closing = k;
                            break;
                        }
                    }
                }
                if (closing != std::string::npos)
                {
                    close_segment();
                    if (double_marker) bold = true; else italic = true;
                    i += width - 1;
                    continue;
                }
            }
            else
            {
                close_segment();
                if (double_marker) bold = false; else italic = false;
                i += width - 1;
                continue;
            }
        }
        out.push_back(c);
    }
    close_segment();
}

bool all_of_char(const std::string &s, char c)
{
    return !s.empty() && s.find_first_not_of(c) == std::string::npos;
}

std::string trim(const std::string &s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Is the line a list marker: -, *, + or 1. followed by a space? Gives the text after it.
bool list_marker(const std::string &line, size_t &text_at)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i + 1 < line.size() && (line[i] == '-' || line[i] == '*' || line[i] == '+') && line[i + 1] == ' ')
    {
        text_at = i + 2;
        return true;
    }
    size_t digits = i;
    while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits]))) ++digits;
    if (digits > i && digits + 1 < line.size() && (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ')
    {
        text_at = digits + 2;
        return true;
    }
    return false;
}

std::vector<Block> parse_markdown(const std::string &s)
{
    std::vector<std::string> lines;
    {
        size_t from = 0;
        while (from <= s.size())
        {
            size_t end = s.find('\n', from);
            if (end == std::string::npos)
            {
                end = s.size();
            }
            std::string line = s.substr(from, end - from);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            lines.push_back(line);
            from = end + 1;
        }
    }

    std::vector<Block> blocks;
    std::vector<bool> tight;
    std::string paragraph;      // raw lines of the paragraph being gathered, joined with spaces
    bool paragraph_quote = false;
    int item_depth = 0;         // above 0 while the paragraph is a list item

    auto add_block = [&](Block::Kind kind, int level, bool quote, bool centered, const std::string &raw, bool markdown_inline) {
        Block block;
        block.kind = kind;
        block.level = level;
        block.quote = quote;
        block.centered = centered;
        if (markdown_inline)
        {
            inline_markdown(raw, block.text, block.spans);
        }
        else
        {
            block.text = raw;
        }
        // trim the ends of the text, keeping spans in range
        while (!block.text.empty() && is_space(block.text.back()))
        {
            block.text.pop_back();
        }
        if (block.text.empty())
        {
            return;
        }
        std::vector<StyleSpan> kept;
        for (StyleSpan span : block.spans)
        {
            span.end = std::min<uint32_t>(span.end, static_cast<uint32_t>(block.text.size()));
            if (span.end > span.start)
            {
                kept.push_back(span);
            }
        }
        block.spans = kept;
        blocks.push_back(std::move(block));
        tight.push_back(false);
    };
    auto flush_paragraph = [&]() {
        if (!paragraph.empty())
        {
            if (item_depth > 0)
            {
                add_block(Block::Kind::ListItem, item_depth, false, false, paragraph, true);
            }
            else
            {
                add_block(Block::Kind::Paragraph, 0, paragraph_quote, false, paragraph, true);
            }
        }
        paragraph.clear();
        item_depth = 0;
        paragraph_quote = false;
    };

    for (size_t li = 0; li < lines.size(); ++li)
    {
        std::string line = lines[li];
        const std::string t = trim(line);

        // fenced code
        if (t.compare(0, 3, "```") == 0 || t.compare(0, 3, "~~~") == 0)
        {
            flush_paragraph();
            const std::string fence = t.substr(0, 3);
            std::string code;
            for (++li; li < lines.size() && trim(lines[li]).compare(0, 3, fence) != 0; ++li)
            {
                if (!code.empty() || !lines[li].empty())
                {
                    code += lines[li] + "\n";
                }
            }
            add_block(Block::Kind::Pre, 0, false, false, code, false);
            continue;
        }
        if (t.empty())
        {
            flush_paragraph();
            continue;
        }
        // headings
        if (t[0] == '#')
        {
            size_t level = 0;
            while (level < t.size() && t[level] == '#') ++level;
            if (level <= 6 && level < t.size() && t[level] == ' ')
            {
                flush_paragraph();
                std::string text = trim(t.substr(level));
                while (!text.empty() && text.back() == '#') text.pop_back();
                add_block(Block::Kind::Heading, static_cast<int>(level), false, level <= 2, trim(text), true);
                continue;
            }
        }
        // rules
        std::string spaced_out;
        for (char c : t)
        {
            if (c != ' ') spaced_out.push_back(c);
        }
        if (spaced_out.size() >= 3 && (all_of_char(spaced_out, '-') || all_of_char(spaced_out, '*') || all_of_char(spaced_out, '_')) && paragraph.empty())
        {
            add_block(Block::Kind::Paragraph, 0, false, true, "* * *", false);
            continue;
        }
        // setext headings: a line of = or - under a line of text
        if (!paragraph.empty() && item_depth == 0 && (all_of_char(t, '=') || all_of_char(t, '-')) && t.size() >= 2)
        {
            const int level = t[0] == '=' ? 1 : 2;
            const std::string text = paragraph;
            paragraph.clear();
            add_block(Block::Kind::Heading, level, false, true, text, true);
            continue;
        }
        // quotes
        if (t[0] == '>')
        {
            std::string text = trim(t.substr(1));
            while (!text.empty() && text[0] == '>') text = trim(text.substr(1));
            if (!paragraph.empty() && !paragraph_quote)
            {
                flush_paragraph();
            }
            paragraph_quote = true;
            if (text.empty())
            {
                flush_paragraph();
                continue;
            }
            paragraph += (paragraph.empty() ? "" : " ") + text;
            continue;
        }
        // list items
        size_t text_at = 0;
        if (list_marker(line, text_at))
        {
            flush_paragraph();
            int indent = 0;
            for (char c : line)
            {
                if (c == ' ') ++indent;
                else if (c == '\t') indent += 4;
                else break;
            }
            item_depth = 1 + indent / 2;
            paragraph = trim(line.substr(text_at));
            if (paragraph.empty())
            {
                item_depth = 0;
            }
            continue;
        }
        // continuation of a paragraph or an item
        if (!paragraph.empty() && paragraph_quote)
        {
            flush_paragraph();
        }
        const bool hard_break = line.size() >= 2 && line[line.size() - 1] == ' ' && line[line.size() - 2] == ' ';
        paragraph += (paragraph.empty() ? "" : (paragraph.back() == '\n' ? "" : " ")) + t;
        if (hard_break)
        {
            paragraph += "\n";
        }
    }
    flush_paragraph();
    return with_gaps(blocks, tight);
}

} // namespace

std::string to_utf8(const std::string &raw)
{
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB
        && static_cast<unsigned char>(raw[2]) == 0xBF)
    {
        return raw.substr(3);
    }
    if (raw.size() >= 2)
    {
        const unsigned char a = static_cast<unsigned char>(raw[0]);
        const unsigned char b = static_cast<unsigned char>(raw[1]);
        const bool little = a == 0xFF && b == 0xFE;
        const bool big = a == 0xFE && b == 0xFF;
        if (little || big)
        {
            std::string out;
            out.reserve(raw.size());
            auto unit_at = [&](size_t i) -> uint32_t {
                const unsigned char lo = static_cast<unsigned char>(raw[little ? i : i + 1]);
                const unsigned char hi = static_cast<unsigned char>(raw[little ? i + 1 : i]);
                return static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 8);
            };
            for (size_t i = 2; i + 1 < raw.size(); i += 2)
            {
                uint32_t unit = unit_at(i);
                if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < raw.size())
                {
                    const uint32_t low = unit_at(i + 2);
                    if (low >= 0xDC00 && low < 0xE000)
                    {
                        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                        i += 2;
                    }
                }
                append_utf8(out, unit);
            }
            return out;
        }
    }
    if (valid_utf8(raw))
    {
        return raw;
    }
    std::string out;
    out.reserve(raw.size() + raw.size() / 8);
    for (char ch : raw)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 0x80 && c < 0xA0)
        {
            append_utf8(out, CP1252[c - 0x80]);
        }
        else
        {
            append_utf8(out, c);
        }
    }
    return out;
}

bool format_for_extension(const std::string &extension, Format &out)
{
    const std::string ext = lower(extension);
    if (ext == ".md" || ext == ".markdown")
    {
        out = Format::Markdown;
        return true;
    }
    return false;
}

std::vector<Block> parse(const std::string &utf8, Format)
{
    return parse_markdown(utf8);
}

// ---------------------------------------------------------------- information

Info read_info(const std::string &head, Format)
{
    Info info;
    const size_t n = head.size();
    {
        // Markdown: the first heading is the title
        size_t from = 0;
        while (from < n)
        {
            size_t end = head.find('\n', from);
            if (end == std::string::npos) end = n;
            const std::string line = trim(head.substr(from, end - from));
            if (line.size() > 2 && line[0] == '#')
            {
                size_t hashes = 0;
                while (hashes < line.size() && line[hashes] == '#') ++hashes;
                if (hashes < line.size() && line[hashes] == ' ')
                {
                    std::string text;
                    std::vector<StyleSpan> spans;
                    inline_markdown(trim(line.substr(hashes)), text, spans);
                    info.title = text;
                    break;
                }
            }
            from = end + 1;
        }
    }
    return info;
}

} // namespace markup
