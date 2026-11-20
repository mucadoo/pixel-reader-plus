#ifndef READING_MODE_H_
#define READING_MODE_H_

#include <optional>
#include <string>

enum class ReadingMode {
    Scroll,
    Page
};

ReadingMode get_next_reading_mode(ReadingMode reading_mode);

std::optional<ReadingMode> decode_reading_mode(const std::string &reading_mode);
std::string encode_reading_mode(ReadingMode reading_mode);

#endif
