#ifndef CBZ_PAGES_H_
#define CBZ_PAGES_H_

#include <filesystem>
#include <string>
#include <vector>

// The picture files of a comic book archive (.cbz), in reading order: file names compared
// with numbers by value, so page2 comes before page10, and ignoring case.
std::vector<std::string> cbz_page_names(const std::filesystem::path &path);

// The format name for the picture loader ("JPG", "PNG" or "GIF") from a file name.
std::string cbz_image_format(const std::string &name);

#endif
