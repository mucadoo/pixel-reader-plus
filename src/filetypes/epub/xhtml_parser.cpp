#include "./xhtml_parser.h"

#include "./epub_doc_addr.h"
#include "./libxml_iter.h"
#include "./xhtml_string_util.h"
#include "./util/str_utils.h"

#include "doc_api/token_addressing.h"

#include <libxml/parser.h>

#include <cstring>
#include <filesystem>
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
    std::vector<int> header_levels;

    DocAddr current_address;

    std::vector<Node> nodes;
    std::set<std::string> unattached_ids;
    std::unordered_map<std::string, DocAddr> &id_to_addr;

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
        emitted.bold = bold_depth > 0;
        emitted.italic = italic_depth > 0;
        emitted.quote = quote_depth > 0;
        emitted.header_level = header_levels.empty() ? 0 : header_levels.back();
        DEBUG_LOG("[node: " << nodes.back().to_string() << "]");
    }

public:
    NodeProcessor(
        DocAddr current_address,
        std::unordered_map<std::string, DocAddr> &id_to_addr
    ) : current_address(current_address), id_to_addr(id_to_addr)
    {
    }

    void on_text_node(xmlNodePtr node, int node_depth)
    {
        DEBUG_LOG("\"" << escape_newlines(node->content) << "\"");

        if (node->content && xmlStrlen(node->content))
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
                (const char*)node->content
            );

            current_address += get_address_width((const char*)node->content);
        }
    }

    void on_enter_element_node(xmlNodePtr node, int node_depth)
    {
        DEBUG_LOG("<node name=\"" << node->name << "\">");

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
                ++table_depth;
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
                emit_node(node_depth, Node::Type::SectionSeparator, node);
                --table_depth;
                break;
            case ElementType::Tr:
                emit_node(node_depth, Node::Type::InlineBreak, node);
                break;
            case ElementType::Td:
                emit_node(node_depth, Node::Type::InlineText, node, SPACE);
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
    bool italic
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
    if ((bold || italic) && result.size() > start)
    {
        StyleSpan span;
        span.start = start;
        span.end = static_cast<uint32_t>(result.size());
        span.bold = bold;
        span.italic = italic;
        if (!spans.empty()
            && spans.back().end == span.start
            && spans.back().bold == span.bold
            && spans.back().italic == span.italic)
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
        append_styled_piece(
            text_out,
            spans_out,
            compact_whitespace(nodes[j].text.c_str()),
            nodes[j].bold,
            nodes[j].italic
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
                        tokens_out.push_back(std::move(token));

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
                        tokens_out.push_back(std::move(token));
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
                        tokens_out.push_back(std::make_unique<ImageDocToken>(
                            address,
                            (base_path / (const char*)img_path).lexically_normal()
                        ));
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
                    tokens_out.push_back(std::make_unique<TextDocToken>(
                        address,
                        ""
                    ));

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

bool parse_xhtml_tokens(const char *xml_str, std::filesystem::path file_path, uint32_t chapter_number, std::vector<std::unique_ptr<DocToken>> &tokens_out, std::unordered_map<std::string, DocAddr> &id_to_addr_out)
{
    xmlDocPtr doc = xmlReadMemory(xml_str, strlen(xml_str), nullptr, nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER);
    if (doc == nullptr)
    {
        std::cerr << "Unable to parse " << file_path << " as xml" << std::endl;
        return false;
    }

    xmlNodePtr node = xmlDocGetRootElement(doc);

    node = elem_first_child(elem_first_by_name(node, BAD_CAST "html"));
    node = elem_first_child(elem_first_by_name(node, BAD_CAST "body"));

    NodeProcessor processor(
        make_address(chapter_number),
        id_to_addr_out
    );
    visit_nodes(node, processor);

    generate_doc_tokens(processor.get_nodes(), file_path.parent_path(), tokens_out);

    xmlFreeDoc(doc);

    return true;
}
