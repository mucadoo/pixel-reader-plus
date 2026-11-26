#ifndef MARKUP_H_
#define MARKUP_H_

#include "doc_api/doc_token.h"

#include <string>
#include <vector>

// Reading books that are not epubs: plain text in other encodings, and Markdown.
// Each is turned into a list of blocks, which the text reader then makes into tokens.
namespace markup
{

// The text as UTF-8. Reads UTF-8 (with or without a byte order mark) and UTF-16 with a
// byte order mark; anything else that is not valid UTF-8 is taken to be Latin-1 / Windows-1252.
std::string to_utf8(const std::string &raw);

struct Block
{
    enum class Kind
    {
        Paragraph,
        Heading,
        ListItem,
        Pre,
        Blank, // the empty line between paragraphs
    };

    Kind kind = Kind::Paragraph;
    int level = 0;        // heading level 1 to 6, or list depth from 1
    bool quote = false;
    bool centered = false;
    std::string text;
    std::vector<StyleSpan> spans;
};

enum class Format
{
    Markdown,
};

// Which format a file name stands for. False for names that are not one of these.
bool format_for_extension(const std::string &extension, Format &out);

std::vector<Block> parse(const std::string &utf8, Format format);

struct Info
{
    std::string title;
    std::string author;
    std::string language;
    std::string series;
    std::string series_index;
};

// What the start of the file says about the book.
Info read_info(const std::string &utf8_head, Format format);

} // namespace markup

#endif
