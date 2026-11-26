#ifndef NEXT_BOOK_H_
#define NEXT_BOOK_H_

#include <filesystem>
#include <functional>
#include <optional>

class BookInfoCache;

// What to read after a book: the next volume of its series, else the next file
// in its folder.
namespace next_book
{

struct Choice
{
    std::filesystem::path path;
    bool by_series = false;
};

// The book that follows `current`, looking at the books in the same folder.
// With series_only, only the next volume of a series counts.
std::optional<Choice> find_next(const std::filesystem::path &current, BookInfoCache &cache, bool series_only);

// Where the small per-book info files are kept, shared by every lookup.
void set_info_dir(const std::filesystem::path &dir);
BookInfoCache &shared_cache();

// A reader that is closing asks for a book to be opened once it is gone.
void set_opener(std::function<void(const std::filesystem::path &)> opener);
void request(const std::filesystem::path &book);
// Opens the requested book, if one was asked for.
void open_requested();

} // namespace next_book

#endif
