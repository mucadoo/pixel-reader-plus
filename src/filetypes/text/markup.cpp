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

// ---------------------------------------------------------------- entities

const char *const LATIN1_NAMES[96] = {
    "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy", "ordf", "laquo", "not", "shy", "reg", "macr",
    "deg", "plusmn", "sup2", "sup3", "acute", "micro", "para", "middot", "cedil", "sup1", "ordm", "raquo", "frac14", "frac12", "frac34", "iquest",
    "Agrave", "Aacute", "Acirc", "Atilde", "Auml", "Aring", "AElig", "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute", "Icirc", "Iuml",
    "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash", "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig",
    "agrave", "aacute", "acirc", "atilde", "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc", "euml", "igrave", "iacute", "icirc", "iuml",
    "eth", "ntilde", "ograve", "oacute", "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave", "uacute", "ucirc", "uuml", "yacute", "thorn", "yuml",
};

bool named_entity(const std::string &name, uint32_t &cp)
{
    static const std::map<std::string, uint32_t> extra = {
        {"amp", 38}, {"lt", 60}, {"gt", 62}, {"quot", 34}, {"apos", 39},
        {"ndash", 8211}, {"mdash", 8212}, {"lsquo", 8216}, {"rsquo", 8217}, {"sbquo", 8218},
        {"ldquo", 8220}, {"rdquo", 8221}, {"bdquo", 8222}, {"dagger", 8224}, {"Dagger", 8225},
        {"bull", 8226}, {"hellip", 8230}, {"permil", 8240}, {"lsaquo", 8249}, {"rsaquo", 8250},
        {"euro", 8364}, {"trade", 8482}, {"oelig", 339}, {"OElig", 338}, {"ensp", 32}, {"emsp", 32}, {"thinsp", 32},
    };
    const auto it = extra.find(name);
    if (it != extra.end())
    {
        cp = it->second;
        return true;
    }
    for (int i = 0; i < 96; ++i)
    {
        if (name == LATIN1_NAMES[i])
        {
            cp = static_cast<uint32_t>(160 + i);
            return true;
        }
    }
    return false;
}

// Replaces &name; and &#number; with the character. A no-break space becomes a plain space.
std::string decode_entities(const std::string &in)
{
    if (in.find('&') == std::string::npos)
    {
        return in;
    }
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] != '&')
        {
            out.push_back(in[i]);
            continue;
        }
        const size_t semi = in.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 10 || semi == i + 1)
        {
            out.push_back(in[i]);
            continue;
        }
        const std::string name = in.substr(i + 1, semi - i - 1);
        uint32_t cp = 0;
        bool ok = false;
        if (name[0] == '#')
        {
            try
            {
                cp = name.size() > 1 && (name[1] == 'x' || name[1] == 'X')
                    ? static_cast<uint32_t>(std::stoul(name.substr(2), nullptr, 16))
                    : static_cast<uint32_t>(std::stoul(name.substr(1)));
                ok = cp > 0 && cp <= 0x10FFFF;
            }
            catch (...)
            {
                ok = false;
            }
        }
        else
        {
            ok = named_entity(name, cp);
        }
        if (!ok)
        {
            out.push_back(in[i]);
            continue;
        }
        if (cp == 160)
        {
            cp = 32;
        }
        append_utf8(out, cp);
        i = semi;
    }
    return out;
}

// ---------------------------------------------------------------- block building

// Collects the text of one block, with the bold and italic runs inside it.
class Builder
{
public:
    std::vector<Block> blocks;
    std::string text;
    std::vector<StyleSpan> spans;
    int bold = 0;
    int italic = 0;
    size_t segment_start = 0;
    bool pre = false;

    void close_segment()
    {
        if ((bold > 0 || italic > 0) && text.size() > segment_start)
        {
            StyleSpan span;
            span.start = static_cast<uint32_t>(segment_start);
            span.end = static_cast<uint32_t>(text.size());
            span.bold = bold > 0;
            span.italic = italic > 0;
            spans.push_back(span);
        }
        segment_start = text.size();
    }

    void change_style(int bold_by, int italic_by)
    {
        close_segment();
        bold = std::max(0, bold + bold_by);
        italic = std::max(0, italic + italic_by);
    }

    bool at_start() const
    {
        return text.empty() || text.back() == '\n';
    }

    // Text with the whitespace rules of HTML, or kept as it is inside pre.
    void add_text(const std::string &raw)
    {
        if (pre)
        {
            for (char c : raw)
            {
                if (c != '\r')
                {
                    text.push_back(c);
                }
            }
            return;
        }
        for (char c : raw)
        {
            if (is_space(c))
            {
                if (!at_start() && text.back() != ' ')
                {
                    text.push_back(' ');
                }
            }
            else
            {
                text.push_back(c);
            }
        }
    }

    void line_break()
    {
        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }
        if (!text.empty() && text.back() != '\n')
        {
            text.push_back('\n');
        }
    }

    // Ends the block being built. Nothing is added when it holds no text.
    void flush(Block::Kind kind, int level, bool quote, bool centered)
    {
        close_segment();
        if (kind != Block::Kind::Pre)
        {
            while (!text.empty() && (text.back() == ' ' || text.back() == '\n'))
            {
                text.pop_back();
            }
        }
        else
        {
            while (!text.empty() && text.back() == '\n')
            {
                text.pop_back();
            }
            size_t lead = 0;
            while (lead < text.size() && text[lead] == '\n')
            {
                ++lead;
            }
            if (lead > 0)
            {
                text.erase(0, lead); // spans were counted from the start, so move them with it
                for (StyleSpan &span : spans)
                {
                    span.start = span.start > lead ? static_cast<uint32_t>(span.start - lead) : 0;
                    span.end = span.end > lead ? static_cast<uint32_t>(span.end - lead) : 0;
                }
            }
        }
        if (!text.empty())
        {
            Block block;
            block.kind = kind;
            block.level = level;
            block.quote = quote;
            block.centered = centered;
            block.text = text;
            for (StyleSpan span : spans)
            {
                if (span.start >= block.text.size())
                {
                    continue;
                }
                span.end = std::min<uint32_t>(span.end, static_cast<uint32_t>(block.text.size()));
                if (span.end > span.start)
                {
                    block.spans.push_back(span);
                }
            }
            blocks.push_back(std::move(block));
        }
        text.clear();
        spans.clear();
        segment_start = 0;
    }
};

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

// ---------------------------------------------------------------- HTML and FB2

struct Markup
{
    Builder builder;
    std::vector<bool> tight_before;   // parallel to builder.blocks
    int heading = 0;
    bool in_title = false;            // an FB2 title: its paragraphs make one heading
    int section_depth = 0;
    int pre_depth = 0;
    int quote_depth = 0;
    int list_depth = 0;
    bool in_item = false;
    bool centered = false;
    bool tight_run = false;           // inside a stanza, where the lines follow each other without a gap
    bool stanza_has_block = false;
    int skip_depth = 0;               // inside head, script, style, description, binary
    std::string skip_name;

    void flush()
    {
        const size_t before = builder.blocks.size();
        Block::Kind kind = Block::Kind::Paragraph;
        int level = 0;
        if (heading > 0)
        {
            kind = Block::Kind::Heading;
            level = heading;
        }
        else if (in_item)
        {
            kind = Block::Kind::ListItem;
            level = std::max(1, list_depth);
        }
        else if (pre_depth > 0)
        {
            kind = Block::Kind::Pre;
        }
        builder.flush(kind, level, quote_depth > 0, centered || (kind == Block::Kind::Heading && level <= 2));
        if (builder.blocks.size() > before)
        {
            tight_before.push_back(tight_run && stanza_has_block);
            if (tight_run)
            {
                stanza_has_block = true;
            }
        }
    }

    void enter_skip(const std::string &name)
    {
        skip_name = name;
        skip_depth = 1;
    }
};

bool is_block_tag(const std::string &name)
{
    static const char *const names[] = {
        "p", "div", "section", "article", "aside", "header", "footer", "nav", "main", "figure", "figcaption", "table", "tr",
        "ul", "ol", "dl", "dt", "dd", "blockquote", "hr", "address", "form", "fieldset", "details", "summary", "body", "html",
        "epigraph", "cite", "poem", "stanza", "v", "subtitle", "text-author", "annotation", "empty-line", "title", "date",
    };
    for (const char *n : names)
    {
        if (name == n)
        {
            return true;
        }
    }
    return false;
}

void handle_tag(Markup &m, const std::string &name, bool closing, bool self_closing)
{
    Builder &b = m.builder;

    if (m.skip_depth > 0)
    {
        if (name == m.skip_name)
        {
            if (closing)
            {
                --m.skip_depth;
            }
            else if (!self_closing)
            {
                ++m.skip_depth;
            }
        }
        return;
    }
    if (!closing && !self_closing && (name == "head" || name == "script" || name == "style" || name == "description"
        || name == "binary" || name == "template" || name == "noscript"))
    {
        m.flush();
        m.enter_skip(name);
        return;
    }

    // Inline styles
    if (name == "b" || name == "strong")
    {
        if (!self_closing) b.change_style(closing ? -1 : 1, 0);
        return;
    }
    if (name == "i" || name == "em" || name == "emphasis" || name == "dfn" || name == "var")
    {
        if (!self_closing) b.change_style(0, closing ? -1 : 1);
        return;
    }
    if (name == "br")
    {
        b.line_break();
        return;
    }

    if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6')
    {
        m.flush();
        m.heading = closing ? 0 : name[1] - '0';
        return;
    }
    if (name == "title")
    {
        // An FB2 title. In HTML the title sits in the head, which is skipped.
        m.flush();
        if (closing)
        {
            m.in_title = false;
            m.heading = 0;
        }
        else
        {
            m.in_title = true;
            m.heading = std::max(1, std::min(6, m.section_depth));
        }
        return;
    }
    if (name == "section")
    {
        m.flush();
        m.section_depth += closing ? -1 : 1;
        m.section_depth = std::max(0, m.section_depth);
        return;
    }
    if (name == "pre")
    {
        m.flush();
        if (closing)
        {
            m.pre_depth = std::max(0, m.pre_depth - 1);
            b.pre = m.pre_depth > 0;
        }
        else
        {
            ++m.pre_depth;
            b.pre = true;
        }
        return;
    }
    if (name == "blockquote" || name == "epigraph" || name == "cite")
    {
        m.flush();
        m.quote_depth = std::max(0, m.quote_depth + (closing ? -1 : 1));
        return;
    }
    if (name == "ul" || name == "ol")
    {
        m.flush();
        m.in_item = false;
        m.list_depth = std::max(0, m.list_depth + (closing ? -1 : 1));
        return;
    }
    if (name == "li")
    {
        m.flush();
        m.in_item = !closing;
        return;
    }
    if (name == "empty-line" || name == "hr")
    {
        m.flush();
        return;
    }
    if (name == "stanza")
    {
        m.flush();
        m.tight_run = !closing;
        m.stanza_has_block = false;
        return;
    }
    if (name == "p" && m.in_title)
    {
        b.add_text(" ");
        return;
    }
    if (name == "v")
    {
        m.flush();
        return;
    }
    if (name == "subtitle")
    {
        m.flush();
        m.centered = !closing;
        if (!closing)
        {
            b.change_style(0, 1);
        }
        else
        {
            b.change_style(0, -1);
        }
        return;
    }
    if (is_block_tag(name))
    {
        m.flush();
        return;
    }
    // anything else (a, span, sub, sup, img, ...) leaves the text as it is
}

void parse_markup(const std::string &s, Markup &m)
{
    size_t i = 0;
    const size_t n = s.size();
    std::string pending;
    auto put_text = [&]() {
        if (m.skip_depth == 0 && !pending.empty())
        {
            m.builder.add_text(decode_entities(pending));
        }
        pending.clear();
    };
    while (i < n)
    {
        if (s[i] != '<')
        {
            const size_t next = s.find('<', i);
            pending.append(s, i, (next == std::string::npos ? n : next) - i);
            i = next == std::string::npos ? n : next;
            continue;
        }
        if (s.compare(i, 4, "<!--") == 0)
        {
            const size_t end = s.find("-->", i + 4);
            i = end == std::string::npos ? n : end + 3;
            continue;
        }
        if (s.compare(i, 9, "<![CDATA[") == 0)
        {
            const size_t end = s.find("]]>", i + 9);
            const std::string data = s.substr(i + 9, (end == std::string::npos ? n : end) - i - 9);
            put_text();
            if (m.skip_depth == 0)
            {
                m.builder.add_text(data);
            }
            i = end == std::string::npos ? n : end + 3;
            continue;
        }
        if (i + 1 < n && (s[i + 1] == '!' || s[i + 1] == '?'))
        {
            const size_t end = s.find('>', i);
            i = end == std::string::npos ? n : end + 1;
            continue;
        }
        if (i + 1 >= n || !(std::isalpha(static_cast<unsigned char>(s[i + 1])) || s[i + 1] == '/'))
        {
            pending.push_back('<');
            ++i;
            continue;
        }

        // A tag. Find its end, skipping over quoted values.
        size_t j = i + 1;
        char quote = 0;
        while (j < n)
        {
            const char c = s[j];
            if (quote)
            {
                if (c == quote) quote = 0;
            }
            else if (c == '"' || c == '\'')
            {
                quote = c;
            }
            else if (c == '>')
            {
                break;
            }
            ++j;
        }
        if (j >= n)
        {
            break;
        }
        std::string inner = s.substr(i + 1, j - i - 1);
        i = j + 1;
        const bool closing = !inner.empty() && inner[0] == '/';
        if (closing)
        {
            inner.erase(0, 1);
        }
        const bool self_closing = !inner.empty() && inner.back() == '/';
        size_t name_end = 0;
        while (name_end < inner.size() && !is_space(inner[name_end]) && inner[name_end] != '/')
        {
            ++name_end;
        }
        std::string name = lower(inner.substr(0, name_end));
        const size_t colon = name.find(':');
        if (colon != std::string::npos)
        {
            name.erase(0, colon + 1);
        }
        put_text();
        handle_tag(m, name, closing, self_closing);

        // Raw text elements run to their end tag
        if (!closing && !self_closing && m.skip_depth > 0 && (name == "script" || name == "style"))
        {
            const std::string end_tag = "</" + name;
            size_t end = s.size();
            for (size_t k = i; k + end_tag.size() <= n; ++k)
            {
                if (lower(s.substr(k, end_tag.size())) == end_tag)
                {
                    end = k;
                    break;
                }
            }
            const size_t close = s.find('>', end);
            i = close == std::string::npos ? n : close + 1;
            m.skip_depth = 0;
        }
    }
    put_text();
    m.flush();
}

std::vector<Block> parse_html_or_fb2(const std::string &s)
{
    Markup m;
    parse_markup(s, m);
    return with_gaps(m.builder.blocks, m.tight_before);
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
    if (ext == ".html" || ext == ".htm" || ext == ".xhtml")
    {
        out = Format::Html;
        return true;
    }
    if (ext == ".fb2")
    {
        out = Format::Fb2;
        return true;
    }
    return false;
}

std::vector<Block> parse(const std::string &utf8, Format format)
{
    if (format == Format::Markdown)
    {
        return parse_markdown(utf8);
    }
    return parse_html_or_fb2(utf8);
}

// ---------------------------------------------------------------- information

namespace
{

// The text inside the first <tag ...>...</tag> at or after from, without inner tags.
bool tag_text(const std::string &s, const std::string &tag, size_t from, size_t to, std::string &out, size_t *end_pos = nullptr)
{
    const std::string lowered = lower(s.substr(0, std::min(s.size(), to)));
    size_t at = from;
    while (true)
    {
        at = lowered.find("<" + tag, at);
        if (at == std::string::npos)
        {
            return false;
        }
        const size_t after = at + 1 + tag.size();
        if (after < lowered.size() && (lowered[after] == '>' || is_space(lowered[after]) || lowered[after] == '/'))
        {
            break;
        }
        at = after;
    }
    const size_t open_end = s.find('>', at);
    if (open_end == std::string::npos)
    {
        return false;
    }
    const size_t close = lowered.find("</" + tag, open_end);
    if (close == std::string::npos)
    {
        return false;
    }
    std::string inner = s.substr(open_end + 1, close - open_end - 1);
    std::string plain;
    bool in_tag = false;
    for (char c : inner)
    {
        if (c == '<') in_tag = true;
        else if (c == '>') in_tag = false;
        else if (!in_tag) plain.push_back(c);
    }
    out = trim(decode_entities(plain));
    if (end_pos)
    {
        *end_pos = close;
    }
    return true;
}

// The value of an attribute inside the tag starting at tag_start.
std::string attribute(const std::string &s, size_t tag_start, const std::string &name)
{
    const size_t end = s.find('>', tag_start);
    if (end == std::string::npos)
    {
        return "";
    }
    const std::string tag = lower(s.substr(tag_start, end - tag_start));
    size_t at = 0;
    while ((at = tag.find(name + "=", at)) != std::string::npos)
    {
        if (at > 0 && !is_space(tag[at - 1]))
        {
            at += name.size();
            continue;
        }
        const size_t value_at = at + name.size() + 1;
        if (value_at >= tag.size())
        {
            return "";
        }
        const char q = tag[value_at];
        if (q == '"' || q == '\'')
        {
            const size_t close = s.find(q, tag_start + value_at + 1);
            return close == std::string::npos ? "" : decode_entities(s.substr(tag_start + value_at + 1, close - tag_start - value_at - 1));
        }
        return "";
    }
    return "";
}

} // namespace

Info read_info(const std::string &head, Format format)
{
    Info info;
    const size_t n = head.size();
    if (format == Format::Fb2)
    {
        const std::string lowered = lower(head);
        size_t title_info = lowered.find("<title-info");
        if (title_info == std::string::npos)
        {
            title_info = 0;
        }
        const size_t title_end = lowered.find("</title-info", title_info);
        const size_t limit = title_end == std::string::npos ? n : title_end;
        tag_text(head, "book-title", title_info, limit, info.title);
        tag_text(head, "lang", title_info, limit, info.language);
        const size_t author = lowered.find("<author", title_info);
        if (author != std::string::npos && author < limit)
        {
            std::string first, last;
            tag_text(head, "first-name", author, limit, first);
            tag_text(head, "last-name", author, limit, last);
            info.author = trim(first + " " + last);
        }
        const size_t sequence = lowered.find("<sequence", title_info);
        if (sequence != std::string::npos && sequence < limit)
        {
            info.series = trim(attribute(head, sequence, "name"));
            info.series_index = trim(attribute(head, sequence, "number"));
        }
    }
    else if (format == Format::Html)
    {
        const std::string lowered = lower(head);
        tag_text(head, "title", 0, n, info.title);
        size_t at = 0;
        while ((at = lowered.find("<meta", at)) != std::string::npos)
        {
            const std::string name = lower(attribute(head, at, "name"));
            if (name == "author")
            {
                info.author = trim(attribute(head, at, "content"));
            }
            else if (name == "language" || name == "dc.language")
            {
                info.language = trim(attribute(head, at, "content"));
            }
            at += 5;
        }
        if (info.language.empty())
        {
            const size_t html = lowered.find("<html");
            if (html != std::string::npos)
            {
                info.language = trim(attribute(head, html, "lang"));
            }
        }
    }
    else
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
