#ifndef HYPHENATION_H_
#define HYPHENATION_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Finds where a word may be broken at the end of a line, with the TeX pattern
// method (Liang). The patterns for a language are read from
// resources/hyphenation/<name>.txt, one pattern per line, and exception words
// with their break points marked by hyphens, such as "as-so-ciate".
class Hyphenator
{
    std::unordered_map<std::u32string, std::vector<uint8_t>> patterns;
    std::unordered_map<std::u32string, std::vector<uint32_t>> exceptions; // letters to the indexes of the breaks
    size_t longest = 0;
    bool loaded_ok = false;
    mutable std::unordered_map<std::string, std::vector<uint32_t>> cache;

public:
    // name is "english", "french" or "portuguese". False when the file is missing.
    bool load(const std::string &name);
    bool ready() const { return loaded_ok; }

    // Byte offsets inside the word where it may be split, smallest first. Empty
    // for words that are not plain letters, and for short words.
    std::vector<uint32_t> points(const std::string &word) const;
};

#endif
