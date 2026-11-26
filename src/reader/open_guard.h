#ifndef OPEN_GUARD_H_
#define OPEN_GUARD_H_

#include <filesystem>
#include <optional>

// Notices when the reader stopped while it was opening a book, or soon after, so the next start does
// not open the same book into the same crash. A small file names the book from the moment it starts
// to open until it is closed.
namespace open_guard
{

void set_file(std::filesystem::path file);

// A book starts to open now.
void begin(const std::filesystem::path &book, long long now);
// The book is open and showing.
void opened(long long now);
// The book was closed, or the reader is leaving normally.
void end();

struct Crash
{
    std::filesystem::path book;
    bool while_opening = false;
};

// The book the last run left behind: it stopped while opening it, or less than 90 seconds after it was
// open. The file is removed. None when the last run ended well or had been reading for longer.
std::optional<Crash> take_crash(long long now);

// The next time the book opens, leave out its styles, fonts and page numbers. Used once.
void plain_next(const std::filesystem::path &book);
bool take_plain(const std::filesystem::path &book);

} // namespace open_guard

#endif
