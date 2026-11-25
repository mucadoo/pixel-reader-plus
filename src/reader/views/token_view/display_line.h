#ifndef DISPLAY_LINE_H_
#define DISPLAY_LINE_H_

#include "doc_api/doc_addr.h"
#include "doc_api/doc_token.h"
#include <filesystem>
#include <string>
#include <vector>

struct DisplayLine
{
    enum class Type
    {
        Text,
        Image,
        ImageRef,
    };

    DocAddr address;
    Type type;
    bool continuation = false;

    DisplayLine(DocAddr address, Type type);
    virtual ~DisplayLine() = default;
};

struct TextLine: public DisplayLine
{
    std::string text;
    bool centered;
    int header_level = 0;
    float font_scale = 1.f;
    bool bold = false;
    bool italic = false;
    bool pre = false;
    bool quote = false;
    int first_indent = 0;       // extra space before the first line of a paragraph, in pixels
    int slot_span = 1;
    uint32_t address_lead = 0;
    bool hyphenated = false;    // the line ends in a hyphen that the book text does not have
    bool paragraph_end = false; // the last line of its paragraph, which justify leaves short
    std::vector<StyleSpan> spans;

    TextLine(DocAddr addr, const std::string& text, bool centered = false);
    virtual ~TextLine() = default;
};

struct ImageLine: public DisplayLine
{
    std::filesystem::path image_path;
    uint32_t num_lines;
    uint32_t width, height;

    ImageLine(
        DocAddr addr,
        const std::filesystem::path& image_path,
        uint32_t num_lines,
        uint32_t width,
        uint32_t height
    );
    virtual ~ImageLine() = default;
};


// Reference to an image starting on previous line
struct ImageRefLine: public DisplayLine
{
    uint32_t offset;

    ImageRefLine(DocAddr addr, uint32_t offset);
    virtual ~ImageRefLine() = default;
};
#endif
