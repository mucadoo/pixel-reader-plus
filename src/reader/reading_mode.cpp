#include "./reading_mode.h"

#include <stdexcept>

namespace
{

const char *SCROLL_STR = "scroll";
const char *PAGE_STR = "page";

} // namespace

ReadingMode get_next_reading_mode(ReadingMode reading_mode)
{
    return reading_mode == ReadingMode::Scroll ? ReadingMode::Page : ReadingMode::Scroll;
}

std::optional<ReadingMode> decode_reading_mode(const std::string &reading_mode)
{
    if (reading_mode == SCROLL_STR)
    {
        return ReadingMode::Scroll;
    }
    if (reading_mode == PAGE_STR)
    {
        return ReadingMode::Page;
    }
    return {};
}

std::string encode_reading_mode(ReadingMode reading_mode)
{
    switch (reading_mode)
    {
        case ReadingMode::Scroll:
            return SCROLL_STR;
        case ReadingMode::Page:
            return PAGE_STR;
        default:
            throw std::runtime_error("Invalid reading mode");
    }
}
