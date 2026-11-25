#ifndef BOOKMARKS_H_
#define BOOKMARKS_H_

#include "doc_api/doc_addr.h"

#include <filesystem>
#include <string>
#include <vector>

// A place the reader marked, with the name shown in the list.
struct Bookmark
{
    DocAddr address = 0;
    std::string label;
};

// Sorted by address. A missing or unreadable file gives an empty list.
std::vector<Bookmark> load_bookmarks(const std::filesystem::path &path);
void save_bookmarks(const std::filesystem::path &path, const std::vector<Bookmark> &bookmarks);

// True when a bookmark lies within [begin, end).
bool bookmark_in_range(const std::vector<Bookmark> &bookmarks, DocAddr begin, DocAddr end);

#endif