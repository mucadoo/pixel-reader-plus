#include "./xhtml_parser.h"

#include "./book_style.h"
#include "./epub_doc_addr.h"
#include "./libxml_iter.h"
#include "./xhtml_string_util.h"
#include "./util/str_utils.h"
#include "util/zip_utils.h"

#include "doc_api/token_addressing.h"
#include "util/utf8.h"

#include <libxml/parser.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <unordered_map>

#define DEBUG 0
#define DEBUG_LOG(msg) if (DEBUG) { std::cerr << std::string(node_depth * 2, ' ') << msg << std::endl; }

namespace {

const std::string SPACE = " ";

bool element_is_blocking(const xmlChar *name)
{
    if (!name)
    {
        return false;
    }
    static const std::set<std::string> blocking_elements = {
        "address", "article", "aside", "blockquote", "canvas", "dd", "div", "dl", "dt",
        "fieldset", "figcaption", "figure", "footer", "form", "h1", "h2", "h3", "h4",
        "h5", "h6", "header", "hgroup", "hr", "li", "main", "nav", "noscript", "ol",
        "output", "p", "pre", "section", "table", "tfoot", "ul", "video", "br"
    };
    return blocking_elements.find((const char*)name) != blocking_elements.end();
}

enum class ElementType
{
    H,
    P,
    Ol,
    Ul,
    Pre,
    Quote,
    Unknown,
    Image,
    Table,
    Tr,
    Td,
};

const std::unordered_map<std::string, ElementType> _elem_name_to_enum {
    {"h1",    ElementType::H},
    {"h2",    ElementType::H},
    {"h3",    ElementType::H},
    {"h4",    ElementType::H},
    {"h5",    ElementType::H},
    {"h6",    ElementType::H},
    {"p",     ElementType::P},
    {"ol",    ElementType::Ol},
    {"ul",    ElementType::Ul},
    {"pre",   ElementType::Pre},
    {"blockquote", ElementType::Quote},
    {"img",   ElementType::Image},
    {"image", ElementType::Image},
    {"table", ElementType::Table},
    {"tr",    ElementType::Tr},
    {"td",    ElementType::Td},
    {"th",    ElementType::Td},
};

ElementType elem_name_to_enum(const xmlChar *elem_name)
{
    if (!elem_name)
    {
        return ElementType::Unknown;
    }
    auto it = _elem_name_to_enum.find((const char*)elem_name);
    if (it != _elem_name_to_enum.end())
    {
        return it->second;
    }
    return ElementType::Unknown;
}

std::string escape_newlines(const xmlChar *str)
{
    std::string result;
    result.reserve(xmlStrlen(str));

    xmlChar c;
    while ((c = *str++))
    {
        if (c == '\n')
        {
            result += "\\n";
        }
        else
        {
            result += c;
        }
    }
    return result;
}

// ---- Tables
//
// A table whose cells are plain text is set as aligned columns in the monospace
// font. Only spaces are added, so the addresses of the words stay what they were.

constexpr int TABLE_CHAR_BUDGET = 44; // characters in a row before the columns are squeezed
constexpr int TABLE_MIN_COLUMN = 6;

size_t char_count(const std::string &text)
{
    size_t count = 0;
    for (const char *p = text.c_str(); *p; p = utf8_step(p))
    {
        ++count;
    }
    return count;
}

// The text cut into pieces of at most width characters, at spaces where it can.
std::vector<std::string> wrap_cell(const std::string &text, size_t width)
{
    std::vector<std::string> lines;
    std::string line;
    size_t line_chars = 0;
    auto flush = [&]() {
        lines.push_back(line);
        line.clear();
        line_chars = 0;
    };
    size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && text[i] == ' ')
        {
            ++i;
        }
        if (i >= text.size())
        {
            break;
        }
        size_t end = i;
        while (end < text.size() && text[end] != ' ')
        {
            ++end;
        }
        std::string word = text.substr(i, end - i);
        i = end;
        size_t word_chars = char_count(word);
        if (line_chars > 0 && line_chars + 1 + word_chars > width)
        {
            flush();
        }
        // A word longer than the column is cut by letters
        while (word_chars > width)
        {
            size_t take = 0;
            const char *p = word.c_str();
            while (take < width)
            {
                p = utf8_step(p);
                ++take;
            }
            const size_t bytes = static_cast<size_t>(p - word.c_str());
            if (line_chars > 0)
            {
                flush();
            }
            line = word.substr(0, bytes);
            line_chars = width;
            flush();
            word = word.substr(bytes);
            word_chars -= width;
        }
        if (line_chars > 0)
        {
            line += ' ';
            ++line_chars;
        }
        line += word;
        line_chars += word_chars;
    }
    if (line_chars > 0 || lines.empty())
    {
        flush();
    }
    return lines;
}

std::string format_table(const std::vector<std::vector<std::string>> &rows)
{
    size_t cols = 0;
    for (const auto &row : rows)
    {
        cols = std::max(cols, row.size());
    }
    std::vector<size_t> widths(cols, 1);
    for (const auto &row : rows)
    {
        for (size_t c = 0; c < row.size(); ++c)
        {
            widths[c] = std::max(widths[c], char_count(row[c]));
        }
    }
    auto total = [&]() {
        size_t sum = cols > 0 ? (cols - 1) * 2 : 0;
        for (size_t w : widths)
        {
            sum += w;
        }
        return sum;
    };
    while (total() > static_cast<size_t>(TABLE_CHAR_BUDGET))
    {
        size_t widest = 0;
        for (size_t c = 1; c < cols; ++c)
        {
            if (widths[c] > widths[widest])
            {
                widest = c;
            }
        }
        if (widths[widest] <= static_cast<size_t>(TABLE_MIN_COLUMN))
        {
            break;
        }
        --widths[widest];
    }

    std::vector<std::vector<std::vector<std::string>>> wrapped; // row, cell, lines
    bool tall = false;
    for (const auto &row : rows)
    {
        std::vector<std::vector<std::string>> cells;
        size_t height = 1;
        for (size_t c = 0; c < cols; ++c)
        {
            cells.push_back(wrap_cell(c < row.size() ? row[c] : std::string(), widths[c]));
            height = std::max(height, cells.back().size());
        }
        tall = tall || height > 1;
        wrapped.push_back(std::move(cells));
    }

    std::string out;
    for (size_t r = 0; r < wrapped.size(); ++r)
    {
        size_t height = 1;
        for (const auto &cell : wrapped[r])
        {
            height = std::max(height, cell.size());
        }
        for (size_t h = 0; h < height; ++h)
        {
            std::string line;
            for (size_t c = 0; c < cols; ++c)
            {
                const std::string piece = h < wrapped[r][c].size() ? wrapped[r][c][h] : std::string();
                line += piece;
                if (c + 1 < cols)
                {
                    line.append(widths[c] - char_count(piece) + 2, ' ');
                }
            }
            if (!out.empty())
            {
                out += '\n';
            }
            out += line;
        }
        if (tall && r + 1 < wrapped.size())
        {
            out += "\n"; // a blank line between rows that take more than one line
        }
    }
    return out;
}

// Whether a table can be set as columns: only text in its cells, no merged cells, and room for two columns.
bool table_is_simple(xmlNodePtr table)
{
    bool two_columns = false;
    std::function<bool(xmlNodePtr, bool)> visit = [&](xmlNodePtr node, bool top) -> bool {
        for (xmlNodePtr child = node ? node->children : nullptr; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE || !child->name)
            {
                continue;
            }
            const char *name = (const char *)child->name;
            static const char *refused[] = {"table", "img", "image", "svg", "ul", "ol", "pre", "blockquote", "h1", "h2", "h3", "h4", "h5", "h6", "math"};
            for (const char *bad : refused)
            {
                if (strcmp(name, bad) == 0)
                {
                    return false;
                }
            }
            if (strcmp(name, "td") == 0 || strcmp(name, "th") == 0)
            {
                for (const char *attr : {"colspan", "rowspan"})
                {
                    xmlChar *value = xmlGetProp(child, BAD_CAST attr);
                    if (value)
                    {
                        const bool merged = std::atoi((const char *)value) > 1;
                        xmlFree(value);
                        if (merged)
                        {
                            return false;
                        }
                    }
                }
            }
            if (strcmp(name, "tr") == 0)
            {
                int cells = 0;
                for (xmlNodePtr cell = child->children; cell; cell = cell->next)
                {
                    if (cell->type == XML_ELEMENT_NODE && cell->name
                        && (strcmp((const char *)cell->name, "td") == 0 || strcmp((const char *)cell->name, "th") == 0))
                    {
                        ++cells;
                    }
                }
                two_columns = two_columns || cells >= 2;
            }
            if (!visit(child, false))
            {
                return false;
            }
        }
        (void)top;
        return true;
    };
    return visit(table, true) && two_columns;
}

// Decorate xml nodes with additional context. Intermediate structure to
// assist conversion between xml nodes and DocTokens.
struct Node
{
    enum class Type
    {
        InlineText,
        InlinePre,
        InlineHeader,
        InlineList,
        InlineBreak,
        SectionSeparator,
        Image,
    };

    Type type;
    DocAddr address;
    xmlNodePtr node;
    std::string text;
    int list_depth;
    bool bold = false;
    bool italic = false;
    bool quote = false;
    int header_level = 0;
    float scale = 1.f;
    float block_scale = 1.f;
    bool block_scale_set = false;
    bool block_bold = false;
    bool block_italic = false;
    bool center = false;
    bool center_set = false;
    bool break_before = false;
    bool break_after = false;
    std::shared_ptr<const std::string> href;

    Node(Type type, DocAddr address, xmlNodePtr node, std::string text, int list_depth)
        : type(type)
        , address(address)
        , node(node)
        , text(std::move(text))
        , list_depth(list_depth)
    {
    }

    bool is_inline() const
    {
        return type == Type::InlineText
            || type == Type::InlinePre
            || type == Type::InlineHeader
            || type == Type::InlineList;
    }

    std::string to_string() const
    {
        std::string typestr;
        switch (type)
        {
            case Type::InlineText:
                typestr = "InlineText";
                break;
            case Type::InlinePre:
                typestr = "InlinePre";
                break;
            case Type::InlineHeader:
                typestr = "InlineHeader";
                break;
            case Type::InlineList:
                typestr = "InlineList";
                break;
            case Type::InlineBreak:
                typestr = "InlineBreak";
                break;
            case Type::SectionSeparator:
                typestr = "SectionSeparator";
                break;
            case Type::Image:
                typestr = "Image";
                break;
            default:
                throw std::runtime_error("Unknown node type");
        }
        return typestr + " " + ::to_string(address);
    }
};

class NodeProcessor
{
    int list_depth = 0;     // depth inside ul/ol tags
    int pre_depth = 0;
    int header_depth = 0;
    int table_depth = 0;
    int bold_depth = 0;
    int italic_depth = 0;
    int quote_depth = 0;
    bool pending_break = false;
    std::vector<int> header_levels;
    const Stylesheet &stylesheet;

    struct Frame
    {
        BookStyle self;
        BookStyle block;
    };
    std::vector<Frame> frames{Frame{}};

    DocAddr current_address;

    std::vector<Node> nodes;
    std::set<std::string> unattached_ids;
    std::unordered_map<std::string, DocAddr> &id_to_addr;
    std::filesystem::path file_path;
    std::vector<std::shared_ptr<const std::string>> link_stack; // one per open <a>

    // A table being gathered into aligned columns
    bool collecting = false;
    DocAddr table_start = 0;
    std::vector<std::vector<std::string>> table_rows;
    std::vector<std::string> table_row;
    std::string table_cell;
    std::string table_preface; // text before the first row, such as a caption
    bool in_cell = false;

    static std::string trimmed(const std::string &text)
    {
        return compact_whitespace(text.c_str());
    }

    void finish_table(int node_depth)
    {
        collecting = false;
        std::string body = format_table(table_rows);
        const std::string preface = trimmed(table_preface);
        if (!preface.empty())
        {
            body = preface + "\n" + body;
        }
        table_rows.clear();
        table_preface.clear();
        if (body.empty())
        {
            return;
        }
        const DocAddr saved = current_address;
        current_address = table_start;
        emit_node(node_depth, Node::Type::InlinePre, nullptr, body);
        current_address = saved;
    }

    void attach_pending_ids(DocAddr address)
    {
        for (const auto &id : unattached_ids)
        {
            id_to_addr[id] = address;
        }
        unattached_ids.clear();
    }

    void emit_node(int node_depth, Node::Type type, xmlNodePtr node, std::string text = "")
    {
        attach_pending_ids(current_address);
        nodes.emplace_back(type, current_address, node, std::move(text), list_depth);
        Node &emitted = nodes.back();
        const Frame &frame = frames.back();
        emitted.bold = frame.self.bold || bold_depth > 0;
        emitted.italic = frame.self.italic || italic_depth > 0;
        emitted.quote = quote_depth > 0;
        emitted.header_level = header_levels.empty() ? 0 : header_levels.back();
        emitted.scale = frame.self.scale;
        emitted.block_scale = frame.block.scale;
        emitted.block_scale_set = frame.block.scale_set;
        emitted.block_bold = frame.block.bold;
        emitted.block_italic = frame.block.italic;
        emitted.center = frame.block.center;
        emitted.center_set = frame.block.center_set;
        bool consumes = type != Node::Type::InlineBreak;
        emitted.break_before = consumes && pending_break;
        if (emitted.break_before)
        {
            pending_break = false;
        }
        emitted.break_after = frame.self.break_after == 1 || frame.block.break_after == 1;
        if (!link_stack.empty())
        {
            emitted.href = link_stack.back();
        }
        DEBUG_LOG("[node: " << nodes.back().to_string() << "]");
    }

public:
    NodeProcessor(
        DocAddr current_address,
        std::unordered_map<std::string, DocAddr> &id_to_addr,
        const Stylesheet &stylesheet,
        BookStyle root = {}
    ) : stylesheet(stylesheet), current_address(current_address), id_to_addr(id_to_addr)
    {
        frames[0].self = root;
        frames[0].block = root;
    }

    void set_file_path(std::filesystem::path path)
    {
        file_path = std::move(path);
    }

    // The target of a link as a path inside the book, so it can be matched
    // against the files of the spine. Outside addresses are kept as they are.
    std::string resolve_href(const std::string &href) const
    {
        if (href.find("://") != std::string::npos || href.rfind("mailto:", 0) == 0)
        {
            return href;
        }
        const size_t hash = href.find('#');
        std::string target = hash == std::string::npos ? href : href.substr(0, hash);
        const std::string fragment = hash == std::string::npos ? std::string() : href.substr(hash + 1);

        // %20 and friends
        std::string decoded;
        for (size_t i = 0; i < target.size(); ++i)
        {
            if (target[i] == '%' && i + 2 < target.size() + 0 && std::isxdigit(static_cast<unsigned char>(target[i + 1]))
                && std::isxdigit(static_cast<unsigned char>(target[i + 2])))
            {
                decoded.push_back(static_cast<char>(std::stoi(target.substr(i + 1, 2), nullptr, 16)));
                i += 2;
            }
            else
            {
                decoded.push_back(target[i]);
            }
        }

        const std::string base = file_path.parent_path().generic_string();
        std::string combined;
        if (decoded.empty())
        {
            combined = file_path.generic_string();
        }
        else if (decoded[0] == '/' || decoded[0] == '\\')
        {
            combined = decoded;
        }
        else
        {
            combined = base.empty() ? decoded : base + "/" + decoded;
        }
        combined = normalize_zip_path(combined);
        return fragment.empty() ? combined : combined + "#" + fragment;
    }

    static std::vector<std::string> class_list(xmlNodePtr node)
    {
        std::vector<std::string> out;
        xmlChar *cls = xmlGetProp(node, BAD_CAST "class");
        if (!cls)
        {
            return out;
        }
        std::string text((const char *)cls);
        xmlFree(cls);
        std::string cur;
        for (char c : text)
        {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                if (!cur.empty())
                {
                    out.push_back(cur);
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
        {
            out.push_back(cur);
        }
        return out;
    }

    void push_style(xmlNodePtr node)
    {
        Frame next = frames.back();
        std::string tag = node->name ? (const char *)node->name : "";
        next.self = stylesheet.apply(next.self, tag, class_list(node));
        if (tag.size() == 2 && tag[0] == 'h' && tag[1] == '1' && !next.self.break_before_set && !(style_tweaks() & TWEAK_PLAIN_BREAKS))
        {
            next.self.break_before = 1;
            next.self.break_before_set = true;
        }
        if (tag == "b" || tag == "strong")
        {
            next.self.bold = true;
        }
        else if (tag == "i" || tag == "em" || tag == "cite")
        {
            next.self.italic = true;
        }
        else if (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6')
        {
            int level = tag[1] - '0';
            next.self.bold = true;
            if (!next.self.scale_set)
            {
                next.self.scale = level == 1 ? 1.5f : level == 2 ? 1.25f : 1.15f;
                next.self.scale_set = true;
            }
            if (!next.self.center_set && level <= 2 && !(style_tweaks() & TWEAK_PLAIN_CENTER))
            {
                next.self.center = true;
                next.self.center_set = true;
            }
        }
        if (next.self.break_before == 1)
        {
            pending_break = true;
        }
        if (element_is_blocking(node->name) && tag != "br")
        {
            next.block = next.self;
        }
        frames.push_back(next);
    }

    void pop_style()
    {
        if (frames.size() > 1)
        {
            frames.pop_back();
        }
    }

    // The last letter of the text so far, to tell an opening quote from a closing one.
    char previous_char = ' ';

    // Curly quotes for the straight ones, one letter for one letter, so addresses do not move.
    std::string smarten(const char *text)
    {
        std::string out;
        const size_t length = std::strlen(text);
        out.reserve(length + 8);
        const auto is_word = [](char c) {
            const unsigned char u = static_cast<unsigned char>(c);
            return std::isalnum(u) || u >= 0x80;
        };
        for (size_t i = 0; i < length; ++i)
        {
            const char c = text[i];
            const char before = out.empty() ? previous_char : out.back();
            const char after = i + 1 < length ? text[i + 1] : ' ';
            const bool opening_context = before == ' ' || before == '\t' || before == '\n' || before == '\r'
                || before == '(' || before == '[' || before == '{' || before == '/'
                || (static_cast<unsigned char>(before) >= 0x80 && !out.empty() && out.size() >= 3
                    && (out.compare(out.size() - 3, 3, "\xE2\x80\x94") == 0 || out.compare(out.size() - 3, 3, "\xE2\x80\x93") == 0));
            if (c == '"')
            {
                out += opening_context ? "\xE2\x80\x9C" : "\xE2\x80\x9D";
            }
            else if (c == '\'')
            {
                if (opening_context && is_word(after))
                {
                    out += "\xE2\x80\x98";
                }
                else
                {
                    out += "\xE2\x80\x99";
                }
            }
            else
            {
                out.push_back(c);
            }
        }
        if (!out.empty())
        {
            previous_char = out.back();
        }
        return out;
    }

    void on_text_node(xmlNodePtr node, int node_depth)
    {
        DEBUG_LOG("\"" << escape_newlines(node->content) << "\"");

        std::string smart;
        const char *content = reinterpret_cast<const char *>(node->content);
        if (content && pre_depth == 0 && (style_tweaks() & TWEAK_SMART_QUOTES))
        {
            smart = smarten(content);
            content = smart.c_str();
        }

        if (collecting)
        {
            if (content)
            {
                if (in_cell)
                {
                    table_cell += content;
                }
                else
                {
                    table_preface += content;
                }
                current_address += get_address_width(content);
            }
            return;
        }

        if (content && *content)
        {
            Node::Type type;
            if (pre_depth > 0)
            {
                type = Node::Type::InlinePre;
            }
            else if (header_depth > 0)
            {
                type = Node::Type::InlineHeader;
            }
            else if (list_depth > 0)
            {
                type = Node::Type::InlineList;
            }
            else
            {
                type = Node::Type::InlineText;
            }

            emit_node(
                node_depth,
                type,
                node,
                content
            );

            current_address += get_address_width(content);
        }
    }

    void on_enter_element_node(xmlNodePtr node, int node_depth)
    {
        DEBUG_LOG("<node name=\"" << node->name << "\">");
        push_style(node);

        // Look for id
        {
            const xmlChar *elem_id = xmlGetProp(node, BAD_CAST "id");
            if (elem_id && xmlStrlen(elem_id) > 0)
            {
                unattached_ids.insert((const char*)elem_id);
            }
        }

        if (element_is_blocking(node->name))
        {
            emit_node(node_depth, Node::Type::InlineBreak, node);
        }

        if (node->name)
        {
            const char *name = (const char *)node->name;
            if (strcmp(name, "b") == 0 || strcmp(name, "strong") == 0)
            {
                ++bold_depth;
            }
            else if (strcmp(name, "i") == 0 || strcmp(name, "em") == 0 || strcmp(name, "cite") == 0)
            {
                ++italic_depth;
            }
            else if (strcmp(name, "a") == 0)
            {
                std::shared_ptr<const std::string> link;
                xmlChar *href = xmlGetProp(node, BAD_CAST "href");
                if (href)
                {
                    if (xmlStrlen(href) > 0)
                    {
                        link = std::make_shared<const std::string>(resolve_href((const char *)href));
                    }
                    xmlFree(href);
                }
                link_stack.push_back(link);
            }
        }

        switch (elem_name_to_enum(node->name))
        {
            case ElementType::H:
                {
                    int level = 1;
                    const char *name = (const char *)node->name;
                    if (name && name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == '\0')
                    {
                        level = name[1] - '0';
                    }
                    header_levels.push_back(level);
                    emit_node(node_depth, Node::Type::SectionSeparator, node);
                    ++header_depth;
                }
                break;
            case ElementType::Quote:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                ++quote_depth;
                break;
            case ElementType::Ol:
            case ElementType::Ul:
                if (list_depth == 0)
                {
                    emit_node(node_depth, Node::Type::SectionSeparator, node);
                }
                ++list_depth;
                break;
            case ElementType::P:
                {
                    bool suppress_blocking = table_depth > 0 || list_depth > 0;
                    if (!suppress_blocking)
                    {
                        emit_node(node_depth, Node::Type::SectionSeparator, node);
                    }
                }
                break;
            case ElementType::Pre:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                ++pre_depth;
                break;
            case ElementType::Table:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                if (table_depth == 0 && table_is_simple(node))
                {
                    collecting = true;
                    table_start = current_address;
                    table_rows.clear();
                    table_row.clear();
                    table_cell.clear();
                    table_preface.clear();
                    in_cell = false;
                }
                ++table_depth;
                break;
            case ElementType::Tr:
                if (collecting)
                {
                    table_row.clear();
                }
                break;
            case ElementType::Td:
                if (collecting)
                {
                    table_cell.clear();
                    in_cell = true;
                }
                break;
            case ElementType::Image:
                emit_node(node_depth, Node::Type::Image, node);
                break;
            default:
                break;
        }
    }

    void on_exit_element_node(xmlNodePtr node, int node_depth)
    {
        DEBUG_LOG("</node name=\"" << node->name << "\">");
        bool break_after = !frames.empty() && frames.back().self.break_after == 1;

        ElementType elem_type = elem_name_to_enum(node->name);
        switch (elem_type)
        {
            case ElementType::H:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                --header_depth;
                if (!header_levels.empty())
                {
                    header_levels.pop_back();
                }
                break;
            case ElementType::Quote:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                if (quote_depth > 0)
                {
                    --quote_depth;
                }
                break;
            case ElementType::Ol:
            case ElementType::Ul:
                --list_depth;
                if (list_depth == 0)
                {
                    emit_node(node_depth, Node::Type::SectionSeparator, node);
                }
                break;
            case ElementType::P:
                {
                    bool suppress_blocking = table_depth > 0 || list_depth > 0;
                    if (!suppress_blocking)
                    {
                        emit_node(node_depth, Node::Type::SectionSeparator, node);
                    }
                }
                break;
            case ElementType::Pre:
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                --pre_depth;
                break;
            case ElementType::Table:
                if (collecting && table_depth == 1)
                {
                    finish_table(node_depth);
                }
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                --table_depth;
                break;
            case ElementType::Tr:
                if (collecting)
                {
                    if (!table_row.empty())
                    {
                        table_rows.push_back(table_row);
                    }
                    table_row.clear();
                }
                else
                {
                    emit_node(node_depth, Node::Type::InlineBreak, node);
                }
                break;
            case ElementType::Td:
                if (collecting)
                {
                    table_row.push_back(trimmed(table_cell));
                    table_cell.clear();
                    in_cell = false;
                }
                else
                {
                    emit_node(node_depth, Node::Type::InlineText, node, SPACE);
                }
                break;
            default:
                break;
        }

        if (element_is_blocking(node->name))
        {
            emit_node(node_depth, Node::Type::InlineBreak, node);
        }

        if (elem_type == ElementType::Image)
        {
            ++current_address;
        }

        if (node->name)
        {
            const char *name = (const char *)node->name;
            if (strcmp(name, "b") == 0 || strcmp(name, "strong") == 0)
            {
                if (bold_depth > 0) --bold_depth;
            }
            else if (strcmp(name, "i") == 0 || strcmp(name, "em") == 0 || strcmp(name, "cite") == 0)
            {
                if (italic_depth > 0) --italic_depth;
            }
            else if (strcmp(name, "a") == 0)
            {
                if (!link_stack.empty()) link_stack.pop_back();
            }
        }
        pop_style();
        if (break_after)
        {
            pending_break = true;
        }
    }

    const std::vector<Node> &get_nodes() const
    {
        return nodes;
    }
};

void visit_nodes(xmlNodePtr node, NodeProcessor &processor, int node_depth = 0)
{
    while (node)
    {
        // Enter handlers
        if (node->type == XML_TEXT_NODE)
        {
            processor.on_text_node(node, node_depth);
        }
        else if (node->type == XML_ELEMENT_NODE)
        {
            processor.on_enter_element_node(node, node_depth);
        }

        // Descend
        visit_nodes(node->children, processor, node_depth + 1);

        // Exit handlers
        if (node->type == XML_ELEMENT_NODE)
        {
            processor.on_exit_element_node(node, node_depth);
        }

        node = node->next;
    }
}

void append_styled_piece(
    std::string &result,
    std::vector<StyleSpan> &spans,
    std::string piece,
    bool bold,
    bool italic,
    float scale,
    const std::shared_ptr<const std::string> &href
)
{
    if (result.empty())
    {
        piece = strip_whitespace_left(piece);
    }

    bool space_left = !result.empty() && is_whitespace(result.back());
    bool space_right = !piece.empty() && is_whitespace(piece.front());
    if (space_left && space_right)
    {
        piece = strip_whitespace_left(piece);
    }

    uint32_t start = static_cast<uint32_t>(result.size());
    result.append(piece);
    if ((bold || italic || scale > 0.2f || href) && result.size() > start)
    {
        StyleSpan span;
        span.start = start;
        span.end = static_cast<uint32_t>(result.size());
        span.bold = bold;
        span.italic = italic;
        span.scale = scale;
        span.href = href;
        if (!spans.empty()
            && spans.back().href == span.href
            && spans.back().end == span.start
            && spans.back().bold == span.bold
            && spans.back().italic == span.italic
            && std::fabs(spans.back().scale - span.scale) < 0.02f)
        {
            spans.back().end = span.end;
        }
        else
        {
            spans.push_back(span);
        }
    }
}

void compact_styled(
    const std::vector<Node> &nodes,
    uint32_t begin,
    uint32_t count,
    std::string &text_out,
    std::vector<StyleSpan> &spans_out
)
{
    text_out.clear();
    spans_out.clear();
    for (uint32_t j = begin; j < begin + count; ++j)
    {
        float scale = 0;
        if (std::fabs(nodes[j].scale - nodes[j].block_scale) > 0.03f)
        {
            scale = nodes[j].scale;
        }
        bool bold = nodes[j].bold && !nodes[j].block_bold;
        bool italic = nodes[j].italic && !nodes[j].block_italic;
        append_styled_piece(
            text_out,
            spans_out,
            compact_whitespace(nodes[j].text.c_str()),
            bold,
            italic,
            scale,
            nodes[j].href
        );
    }

    if (!text_out.empty() && is_whitespace(text_out.back()))
    {
        text_out = strip_whitespace_right(text_out);
        while (!spans_out.empty() && spans_out.back().start >= text_out.size())
        {
            spans_out.pop_back();
        }
        if (!spans_out.empty() && spans_out.back().end > text_out.size())
        {
            spans_out.back().end = static_cast<uint32_t>(text_out.size());
        }
    }
}

// Merge inline text types, emit DocTokens
void generate_doc_tokens(
    const std::vector<Node> &nodes,
    const std::filesystem::path &base_path,
    std::vector<std::unique_ptr<DocToken>> &tokens_out
)
{
    auto get_group_size = [&nodes](uint32_t i) -> uint32_t {
        const auto &head = nodes[i];
        if (head.is_inline())
        {
            Node::Type head_type = head.type;

            uint32_t size = 1;
            while (++i < nodes.size() && nodes[i].type == head_type)
            {
                ++size;
            }
            return size;
        }
        return 1;
    };

    uint32_t n = nodes.size();
    tokens_out.reserve(n);

    bool separator_allowed = true;
    bool carry_break = false;
    auto push_token = [&](std::unique_ptr<DocToken> token, const Node &head) {
        if (carry_break || head.break_before)
        {
            token->page_break_before = true;
        }
        token->page_break_after = head.break_after;
        carry_break = head.break_after;
        tokens_out.push_back(std::move(token));
    };
    uint32_t i = 0;
    while (i < n)
    {
        uint32_t group_size = get_group_size(i);
        const Node &head = nodes[i];
        DocAddr address = head.address;

        switch (head.type)
        {
            case Node::Type::InlineText:
            case Node::Type::InlineHeader:
            case Node::Type::InlineList:
                {
                    std::string text;
                    std::vector<StyleSpan> spans;
                    compact_styled(nodes, i, group_size, text, spans);
                    if (text.size())
                    {
                        std::unique_ptr<DocToken> token;
                        if (head.type == Node::Type::InlineText)
                        {
                            token = std::make_unique<TextDocToken>(address, text);
                        }
                        else if (head.type == Node::Type::InlineHeader)
                        {
                            token = std::make_unique<HeaderDocToken>(address, text);
                            token->header_level = head.header_level > 0 ? head.header_level : 1;
                        }
                        else
                        {
                            token = std::make_unique<ListItemDocToken>(
                                address,
                                text,
                                head.list_depth
                            );
                        }
                        token->spans = std::move(spans);
                        token->quote = head.quote;
                        token->font_scale = head.block_scale > 0.2f ? head.block_scale : 1.f;
                        token->scale_set = head.block_scale_set;
                        token->centered = head.center;
                        token->center_set = head.center_set;
                        token->bold = head.block_bold;
                        token->italic = head.block_italic;
                        push_token(std::move(token), head);

                        separator_allowed = true;
                    }
                }
                break;
            case Node::Type::InlinePre:
                {
                    std::vector<const char *> substrings;
                    substrings.reserve(group_size);
                    for (uint32_t j = i; j < i + group_size; ++j)
                    {
                        substrings.push_back(nodes[j].text.c_str());
                    }

                    std::string text = remove_carriage_returns(join_strings(substrings));
                    if (text.size())
                    {
                        auto token = std::make_unique<TextDocToken>(address, text);
                        token->pre = true;
                        token->font_scale = head.block_scale > 0.2f ? head.block_scale : 1.f;
                        token->scale_set = head.block_scale_set;
                        token->bold = head.block_bold;
                        token->italic = head.block_italic;
                        push_token(std::move(token), head);
                        separator_allowed = true;
                    }
                }
                break;
            case Node::Type::Image:
                {
                    xmlNodePtr node = head.node;
                    const xmlChar *img_path = xmlGetProp(node, BAD_CAST "href");
                    if (!img_path) img_path = xmlGetProp(node, BAD_CAST "src");
                    if (img_path)
                    {
                        std::string rel((const char *)img_path);
                        std::string base = base_path.generic_string();
                        std::string combined = (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
                            ? rel
                            : (base.empty() ? rel : base + "/" + rel);
                        push_token(
                            std::make_unique<ImageDocToken>(
                                address,
                                normalize_zip_path(combined)
                            ),
                            head
                        );
                    }
                    else
                    {
                        std::cerr << "Unable to get link from image" << std::endl;
                    }

                    separator_allowed = true;
                }
                break;
            case Node::Type::SectionSeparator:
                if (separator_allowed)
                {
                    push_token(
                        std::make_unique<TextDocToken>(
                            address,
                            ""
                        ),
                        head
                    );

                    separator_allowed = false;
                }
                break;
            case Node::Type::InlineBreak:
                break;
            default:
                throw std::runtime_error("Unknown node type");
        }
        i += group_size;
    }
}

} // namespace

bool parse_xhtml_tokens(const char *xml_str, std::filesystem::path file_path, uint32_t chapter_number, std::vector<std::unique_ptr<DocToken>> &tokens_out, std::unordered_map<std::string, DocAddr> &id_to_addr_out, EpubResourceLoader load_resource)
{
    xmlDocPtr doc = xmlReadMemory(xml_str, strlen(xml_str), nullptr, nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER);
    if (doc == nullptr)
    {
        std::cerr << "Unable to parse " << file_path << " as xml" << std::endl;
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr html = elem_first_child(elem_first_by_name(root, BAD_CAST "html"));

    Stylesheet stylesheet;
    if (load_resource)
    {
        xmlNodePtr head = elem_first_by_name(html, BAD_CAST "head");
        for (xmlNodePtr child = elem_first_child(head); child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE || !child->name)
            {
                continue;
            }
            if (xmlStrEqual(child->name, BAD_CAST "link"))
            {
                xmlChar *rel = xmlGetProp(child, BAD_CAST "rel");
                xmlChar *href = xmlGetProp(child, BAD_CAST "href");
                bool is_style = rel && xmlStrcasestr(rel, BAD_CAST "stylesheet");
                if (is_style && href)
                {
                    std::string base = file_path.parent_path().generic_string();
                    std::string rel_href((const char *)href);
                    std::string combined = (!rel_href.empty() && (rel_href[0] == '/' || rel_href[0] == '\\'))
                        ? rel_href
                        : (base.empty() ? rel_href : base + "/" + rel_href);
                    auto css = load_resource(normalize_zip_path(combined));
                    if (!css.empty())
                    {
                        stylesheet.parse(std::string(css.data(), css.size()));
                    }
                }
                if (rel) xmlFree(rel);
                if (href) xmlFree(href);
            }
            else if (xmlStrEqual(child->name, BAD_CAST "style"))
            {
                std::string css;
                for (xmlNodePtr text = child->children; text; text = text->next)
                {
                    if (text->type == XML_TEXT_NODE && text->content)
                    {
                        css += (const char *)text->content;
                    }
                }
                if (!css.empty())
                {
                    stylesheet.parse(css);
                }
            }
        }
    }

    xmlNodePtr body = elem_first_by_name(html, BAD_CAST "body");
    BookStyle root_style;
    if (body)
    {
        std::vector<std::string> classes;
        xmlChar *cls = xmlGetProp(body, BAD_CAST "class");
        if (cls)
        {
            std::string text((const char *)cls);
            xmlFree(cls);
            std::string cur;
            for (char c : text)
            {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                {
                    if (!cur.empty())
                    {
                        classes.push_back(cur);
                        cur.clear();
                    }
                }
                else
                {
                    cur.push_back(c);
                }
            }
            if (!cur.empty())
            {
                classes.push_back(cur);
            }
        }
        root_style = stylesheet.apply(BookStyle{}, "body", classes);
    }
    xmlNodePtr node = elem_first_child(body);

    NodeProcessor processor(
        make_address(chapter_number),
        id_to_addr_out,
        stylesheet,
        root_style
    );
    processor.set_file_path(file_path);
    visit_nodes(node, processor);

    generate_doc_tokens(processor.get_nodes(), file_path.parent_path(), tokens_out);

    xmlFreeDoc(doc);

    return true;
}
