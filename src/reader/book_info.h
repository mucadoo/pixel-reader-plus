#ifndef BOOK_INFO_H_
#define BOOK_INFO_H_

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

// What an epub says about itself. Any field can be empty.
struct BookInfo
{
    std::string title;
    std::string author;
    std::string language;
    std::string series;
    // The number in the series as the book writes it, such as "2" or "1.5". Empty when there is none.
    std::string series_index;
    // The blurb, publisher and publication date, when the book gives them (epub). One line each.
    std::string description;
    std::string publisher;
    std::string date;
    // The subjects (genres, tags) the book lists, separated by "; ".
    std::string subjects;

    bool empty() const { return title.empty() && author.empty() && language.empty() && series.empty(); }

    // The series number as a number, for sorting. 0 when there is none.
    double series_number() const;
};

// Reads the title, first author and language from an epub's package file.
// None for other file types, or when the book cannot be read.
std::optional<BookInfo> read_epub_info(const std::filesystem::path &book);

// The title to show for a book: the epub's own, else the file name without its extension.
std::string book_display_title(const std::optional<BookInfo> &info, const std::filesystem::path &book);

// Remembers what read_epub_info found, in memory and in a small file per book,
// so a folder is only read once and later visits are instant.
class BookInfoCache
{
    std::filesystem::path dir;
    std::unordered_map<std::string, std::optional<BookInfo>> known; // by book path

    std::string cache_key(const std::filesystem::path &book) const;

public:
    explicit BookInfoCache(std::filesystem::path dir);

    // True once this book has been looked at, with or without a result.
    bool has(const std::filesystem::path &book) const;
    // What was found for the book, if anything.
    const BookInfo *get(const std::filesystem::path &book) const;
    // Looks at the book now: its cache file, else the epub itself.
    void load(const std::filesystem::path &book);
};

#endif
