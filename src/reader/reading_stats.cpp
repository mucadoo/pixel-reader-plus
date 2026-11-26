#include "./reading_stats.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <vector>
#include <fstream>
#include <map>
#include <system_error>

namespace reading_stats
{

namespace
{

constexpr long long AWAY_AFTER_SECONDS = 240;

std::filesystem::path stats_file;
// day ("2026-11-25"), then book path, to seconds
std::map<std::pair<std::string, std::string>, long long> seconds;
bool dirty = false;
long long last_activity = -1;

std::string day_key(long long when)
{
    const std::time_t t = static_cast<std::time_t>(when);
    std::tm local {};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char text[16];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
    return text;
}

} // namespace

void load(std::filesystem::path file)
{
    stats_file = std::move(file);
    seconds.clear();
    std::ifstream in(stats_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t first = line.find('\t');
        const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos)
        {
            continue;
        }
        try
        {
            seconds[{line.substr(0, first), line.substr(second + 1)}] = std::stoll(line.substr(first + 1, second - first - 1));
        }
        catch (...)
        {
        }
    }
    dirty = false;
}

void flush()
{
    if (!dirty || stats_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(stats_file.parent_path(), ec);
    std::ofstream out(stats_file, std::ios::trunc);
    for (const auto &item : seconds)
    {
        out << item.first.first << '\t' << item.second << '\t' << item.first.second << '\n';
    }
    dirty = false;
}

void reset()
{
    last_activity = -1;
}

void note_activity(const std::filesystem::path &book, long long now)
{
    if (last_activity >= 0)
    {
        const long long gap = now - last_activity;
        if (gap > 0 && gap < AWAY_AFTER_SECONDS)
        {
            seconds[{day_key(now), book.string()}] += gap;
            dirty = true;
        }
    }
    last_activity = now;
}

std::string day_for(long long when)
{
    return day_key(when);
}

long long seconds_on_day(const std::string &day)
{
    long long total = 0;
    for (auto it = seconds.lower_bound({day, std::string()}); it != seconds.end() && it->first.first == day; ++it)
    {
        total += it->second;
    }
    return total;
}

std::vector<std::pair<std::filesystem::path, long long>> books_on_day(const std::string &day)
{
    std::vector<std::pair<std::filesystem::path, long long>> books;
    for (auto it = seconds.lower_bound({day, std::string()}); it != seconds.end() && it->first.first == day; ++it)
    {
        books.emplace_back(std::filesystem::path(it->first.second), it->second);
    }
    std::sort(books.begin(), books.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    return books;
}

Summary summarize(const std::filesystem::path &book, long long now)
{
    Summary summary;
    const std::string today = day_key(now);
    std::vector<std::string> week;
    for (int back = 0; back < 7; ++back)
    {
        week.push_back(day_key(now - static_cast<long long>(back) * 86400));
    }
    for (const auto &item : seconds)
    {
        const bool mine = item.first.second == book.string();
        summary.all_total += item.second;
        if (mine)
        {
            summary.book_total += item.second;
        }
        if (item.first.first == today)
        {
            summary.all_today += item.second;
            if (mine)
            {
                summary.book_today += item.second;
            }
        }
        if (std::find(week.begin(), week.end(), item.first.first) != week.end())
        {
            summary.all_week += item.second;
        }
    }
    return summary;
}

std::string format_duration(long long total)
{
    if (total < 60)
    {
        return total <= 0 ? std::string("0m") : std::string("<1m");
    }
    const long long minutes = total / 60;
    if (minutes < 60)
    {
        return std::to_string(minutes) + "m";
    }
    char text[24];
    std::snprintf(text, sizeof(text), "%lldh %02lldm", minutes / 60, minutes % 60);
    return text;
}

std::string report(const std::filesystem::path &book, long long now, int books_started, int books_finished)
{
    const Summary s = summarize(book, now);
    std::string out;
    out += "This book\n";
    out += "Today  " + format_duration(s.book_today) + "\n";
    out += "Total  " + format_duration(s.book_total) + "\n\n";
    out += "All books\n";
    out += "Today  " + format_duration(s.all_today) + "\n";
    out += "Last 7 days  " + format_duration(s.all_week) + "\n";
    out += "Total  " + format_duration(s.all_total) + "\n\n";
    out += "Books started  " + std::to_string(books_started) + "\n";
    out += "Books finished  " + std::to_string(books_finished);
    return out;
}

} // namespace reading_stats
