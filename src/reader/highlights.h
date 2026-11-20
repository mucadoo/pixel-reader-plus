#ifndef HIGHLIGHTS_H_
#define HIGHLIGHTS_H_

#include "doc_api/doc_addr.h"

#include <filesystem>
#include <utility>
#include <vector>

using Highlight = std::pair<DocAddr, DocAddr>;

std::vector<Highlight> load_highlights(const std::filesystem::path &path);
void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights);
bool highlights_cover(const std::vector<Highlight> &highlights, DocAddr begin);
void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end);

#endif
