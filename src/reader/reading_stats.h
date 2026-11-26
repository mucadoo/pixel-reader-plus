#ifndef READING_STATS_H_
#define READING_STATS_H_

#include <filesystem>
#include <functional>
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

// The day as "2026-11-25" for a moment (seconds since 1970), in local time.
std::string day_for(long long when);

// The time spent reading on one day ("2026-11-25"), in all books.
long long seconds_on_day(const std::string &day);

// The books read on one day, with the seconds in each, the longest first.
std::vector<std::pair<std::filesystem::path, long long>> books_on_day(const std::string &day);

// 3725 seconds as "1h 02m", 540 as "9m".
std::string format_duration(long long seconds);

// The text of the statistics screen for a book.
std::string report(const std::filesystem::path &book, long long now, int books_started, int books_finished);

// The longer statistics screen: this book (when one is given) with its reading speed, the totals over
// 7 and 30 days, the best day, a chart of the last 14 days, the most read books and the books finished
// each month. finished_times are the last-opened moments (seconds since 1970) of the finished books.
// book_units_read is how far into the book the reader is, in address units, 0 when unknown.
std::string detailed_report(
    const std::filesystem::path &book,
    long long now,
    int books_started,
    int books_finished,
    const std::vector<long long> &finished_times,
    long long book_units_read,
    const std::function<std::string(const std::string &)> &title_for
);

} // namespace reading_stats

#endif
