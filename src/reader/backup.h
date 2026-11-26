#ifndef BACKUP_H_
#define BACKUP_H_

#include <filesystem>
#include <string>

// Everything the reader keeps about you and your books (settings, place in each book, library
// history, collections, word list, reading stats, bookmarks, highlights and per-book layouts) in
// one zip file, and the way back. Covers, book info and page caches are left out: they are made
// again on demand.
namespace backup
{

struct Result
{
    bool ok = false;
    int files = 0;
    std::string message;
};

// Writes the zip, replacing a file that is already there.
Result write(const std::filesystem::path &store_dir, const std::filesystem::path &zip_file);

// Puts the files of a zip made by write() back in the store, replacing the ones that are there.
Result restore(const std::filesystem::path &store_dir, const std::filesystem::path &zip_file);

} // namespace backup

#endif
