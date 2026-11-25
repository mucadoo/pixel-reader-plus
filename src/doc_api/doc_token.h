#ifndef DOC_TOKEN_H_
#define DOC_TOKEN_H_

#include "./doc_addr.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct StyleSpan
{
    uint32_t start = 0;
    uint32_t end = 0;
    bool bold = false;
    bool italic = false;
    // 0 inherits the line size. Otherwise this is the size relative to the user's font (1 = body).
    float scale = 0;
    // Where the text links to: a path inside the book with an optional #fragment,
    // or an outside address. Null when the text is not a link.
    std::shared_ptr<const std::string> href;
};

enum class TokenType
{
    Text,
    Header,
    Image,
    ListItem,
};

struct DocToken
{
    TokenType type;
    DocAddr address;

    DocToken(TokenType type, DocAddr address);
    virtual bool operator==(const DocToken &other) const;
    virtual std::string to_string() const = 0;

    // Visual only. Addresses stay tied to the text, so these are not part of equality.
    std::vector<StyleSpan> spans;
    int header_level = 0;
    bool pre = false;
    bool quote = false;
    // Size relative to the user's font. 1 is body text. Set from the book's stylesheet.
    float font_scale = 1.f;
    bool scale_set = false;
    bool centered = false;
    bool center_set = false;
    bool bold = false;
    bool italic = false;
    // Page mode starts a new page around this block. Not part of equality.
    bool page_break_before = false;
    bool page_break_after = false;

protected:
    std::string common_to_string(std::string data) const;
};

struct TextDocToken : public DocToken
{
    std::string text;

    TextDocToken(DocAddr address, const std::string &text);
    bool operator==(const DocToken &other) const override;
    std::string to_string() const override;
};

struct HeaderDocToken : public DocToken
{
    std::string text;

    HeaderDocToken(DocAddr address, const std::string &text);
    bool operator==(const DocToken &other) const override;
    std::string to_string() const override;
};

struct ImageDocToken : public DocToken
{
    std::filesystem::path path;

    ImageDocToken(DocAddr address, const std::filesystem::path &path);
    bool operator==(const DocToken &other) const override;
    std::string to_string() const override;
};

struct ListItemDocToken : public DocToken
{
    std::string text;
    int nest_level;

    ListItemDocToken(DocAddr address, const std::string &text, int nest_level);
    bool operator==(const DocToken &other) const override;
    std::string to_string() const override;
};

std::string to_string(TokenType type);

#endif
