#ifndef LIBRARY_LOG_H_
#define LIBRARY_LOG_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>

// What the reader remembers about each book it has opened: how far along it is
// and when it was last open. The book list uses it for progress, sorting and
// Continue reading.
class LibraryLog
{
public:
    struct Entry
    {
        int percent = 0;
        long long last_opened = 0;
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

    const Entry *get(const std::filesystem::path &book) const;
    // The book opened last that still exists, if any.
    std::optional<std::filesystem::path> most_recent() const;
};

LibraryLog &library_log();

#endif