#ifndef ACCENT_FOLD_H_
#define ACCENT_FOLD_H_

#include <cstdint>
#include <string>
#include <vector>

// The text with accented Latin letters replaced by their plain ones (e with an acute accent becomes e,
// the German sharp s becomes ss), so a search for "cafe" finds "café". Capitals stay capitals, and
// every other letter is left alone. When map is given it gets, for every byte of the result, the offset in
// the original where the letter it came from starts.
std::string fold_accents(const std::string &text, std::vector<uint32_t> *map = nullptr);

#endif