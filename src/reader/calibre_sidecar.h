#ifndef CALIBRE_SIDECAR_H_
#define CALIBRE_SIDECAR_H_

#include <filesystem>
#include <optional>

// Calibre keeps a book's metadata and cover in files beside it: metadata.opf and cover.jpg in the
// book's own folder. A file named like the book (Title.opf, Title.jpg) works too.
namespace calibre_sidecar
{

// The package file for the book, if there is one. A metadata.opf or cover.jpg is taken only when the
// book is the only one in its folder, since in a shared folder it would belong to no book in particular.
std::optional<std::filesystem::path> opf_for(const std::filesystem::path &book);

// The cover picture for the book, if there is one.
std::optional<std::filesystem::path> cover_for(const std::filesystem::path &book);

} // namespace calibre_sidecar

#endif