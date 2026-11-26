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
        Reading,
        ToRead,
        OnHold
    };

    struct Entry
    {
        int percent = 0;
        long long last_opened = 0;
        Status status = Status::Auto;
        bool favorite = false;
        int rating = 0;      // 0 for none, else 1 to 5 stars
        uint32_t units = 0;  // length of the book in address units, 0 until it has been opened
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
    void set_units(const std::filesystem::path &book, uint32_t units);
    // The books opened most recently that still exist, newest first.
    std::vector<std::filesystem::path> recent(size_t max_count) const;
    void set_book_id(const std::filesystem::path &book, const std::string &book_id);
    // Forgets the book, or moves what is known about it to a new path.
    void remove(const std::filesystem::path &book);
    void rename(const std::filesystem::path &from, const std::filesystem::path &to);
    void set_favorite(const std::filesystem::path &book, bool favorite);
    void set_rating(const std::filesystem::path &book, int rating);

    const Entry *get(const std::filesystem::path &book) const;
    // The book opened last that still exists, if any.
    std::optional<std::filesystem::path> most_recent() const;
    // How many books have been opened, and how many of them are finished.
    int started_count() const;
    int finished_count() const;
    // Every book the log knows about that still exists, with what it knows, in path order.
    std::vector<std::pair<std::filesystem::path, Entry>> all() const;
    // The favorite books that still exist, in the order of their path.
    std::vector<std::filesystem::path> favorites() const;
};

LibraryLog &library_log();

#endif