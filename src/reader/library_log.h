#ifndef LIBRARY_LOG_H_
#define LIBRARY_LOG_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// What the reader remembers about each book it has opened: how far along it is
// and when it was last open. The book list uses it for progress, sorting and
// Continue reading.
class LibraryLog
{
public:
    // How the reader counts a book as read. Auto follows the percent.
    enum class Status {
        Auto,
        Finished,
        Reading
    };

    struct Entry
    {
        int percent = 0;
        long long last_opened = 0;
        Status status = Status::Auto;
        bool favorite = false;
        std::string book_id; // names the files kept for the book, empty until it has been opened

        bool finished() const
        {
            return status == Status::Finished || (status == Status::Auto && percent >= 99);
        }
    };

private:
    std::filesystem::path file;
    std::map<std::string, Entry> entries;
    bool dirty = false;

public:
    // Reads the file if it exists. Changes are written by flush().
    void load(std::filesystem::path path);
    void flush();

    // The book was opened now.
    void touch(const std::filesystem::path &book, long long now);
    void set_percent(const std::filesystem::path &book, int percent);
    void set_status(const std::filesystem::path &book, Status status);
    void set_book_id(const std::filesystem::path &book, const std::string &book_id);
    // Forgets the book, or moves what is known about it to a new path.
    void remove(const std::filesystem::path &book);
    void rename(const std::filesystem::path &from, const std::filesystem::path &to);
    void set_favorite(const std::filesystem::path &book, bool favorite);

    const Entry *get(const std::filesystem::path &book) const;
    // The book opened last that still exists, if any.
    std::optional<std::filesystem::path> most_recent() const;
    // How many books have been opened, and how many of them are finished.
    int started_count() const;
    int finished_count() const;
    // The favorite books that still exist, in the order of their path.
    std::vector<std::filesystem::path> favorites() const;
};

LibraryLog &library_log();

#endif