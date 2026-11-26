#ifndef FONT_CATALOG_H_
#define FONT_CATALOG_H_

#include <string>
#include <vector>

std::string get_valid_font_name(const std::string &preferred_font_name);
std::string get_prev_font_name(const std::string &font_name);
std::string get_next_font_name(const std::string &font_name);

// Fonts to draw letters with that the chosen font lacks: any font in the font folder whose
// name starts with "fallback", then the monospace font that comes with the reader.
const std::vector<std::string> &fallback_font_paths();

#endif
