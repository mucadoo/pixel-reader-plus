#ifndef READING_STATS_H_
#define READING_STATS_H_

#include <filesystem>
#include <string>
#include <vector>

// How long the reader has spent in each book, day by day. Time is counted
// between key presses, and a pause of four minutes or more counts as the
// reader being away.
namespace reading_stats
{

void load(std::filesystem::path file);
void flush();

// A book was opened: the next key press does not count the time since the last book.
void reset();

// A key was pressed in the book at this moment (seconds since 1970).
void note_activity(const std::filesystem::path &book, long long now);

struct Summary
{
    long long book_today = 0;
    long long book_total = 0;
    long long all_today = 0;
    long long all_week = 0;
    long long all_total = 0;
};

Summary summarize(const std::filesystem::path &book, long long now);

// 3725 seconds as "1h 02m", 540 as "9m".
std::string format_duration(long long seconds);

// The text of the statistics screen for a book.
std::string report(const std::filesystem::path &book, long long now, int books_started, int books_finished);

} // namespace reading_stats

#endif
