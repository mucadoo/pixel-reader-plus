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

long long average_reading_day(long long now)
{
    std::map<std::string, long long> per_day;
    for (const auto &item : seconds)
    {
        per_day[item.first.first] += item.second;
    }
    long long total = 0;
    int days = 0;
    for (int back = 0; back < 30; ++back)
    {
        const auto found = per_day.find(day_key(now - static_cast<long long>(back) * 86400));
        if (found != per_day.end() && found->second > 0)
        {
            total += found->second;
            ++days;
        }
    }
    return days > 0 ? total / days : 0;
}

long long days_to_finish(long long seconds_left, long long average_day)
{
    if (seconds_left < 0 || average_day <= 0)
    {
        return 0;
    }
    const long long days = (seconds_left + average_day - 1) / average_day;
    return days < 1 ? 1 : days;
}

std::string finish_text(long long seconds_left, long long now)
{
    const long long days = days_to_finish(seconds_left, average_reading_day(now));
    if (days <= 0)
    {
        return "";
    }
    return "about " + std::to_string(days) + (days == 1 ? " day" : " days") + ", around " + day_key(now + days * 86400);
}

std::string detailed_report(
    const std::filesystem::path &book,
    long long now,
    int books_started,
    int books_finished,
    const std::vector<long long> &finished_times,
    long long book_units_read,
    const std::function<std::string(const std::string &)> &title_for,
    long long book_units_total
)
{
    const Summary s = summarize(book, now);
    std::map<std::string, long long> per_day;
    std::map<std::string, long long> per_book;
    for (const auto &item : seconds)
    {
        per_day[item.first.first] += item.second;
        per_book[item.first.second] += item.second;
    }

    std::string out;
    if (!book.empty())
    {
        out += "This book\n";
        out += "Today  " + format_duration(s.book_today) + "\n";
        out += "Total  " + format_duration(s.book_total) + "\n";
        // Six letters make about one word, so address units over six are words.
        if (s.book_total >= 300 && book_units_read > 0)
        {
            const double words_per_minute = (static_cast<double>(book_units_read) / 6.0) / (static_cast<double>(s.book_total) / 60.0);
            char text[48];
            std::snprintf(text, sizeof(text), "Speed  about %d words a minute\n", static_cast<int>(words_per_minute + 0.5));
            out += text;
            if (book_units_total > book_units_read)
            {
                // The time still to read at the pace of this book, and when that comes if you keep reading like this.
                const long long left = static_cast<long long>(static_cast<double>(book_units_total - book_units_read) * static_cast<double>(s.book_total) / static_cast<double>(book_units_read));
                const std::string finish = finish_text(left, now);
                if (!finish.empty())
                {
                    out += "Time left  " + format_duration(left) + "\n";
                    out += "Finish  " + finish + "\n";
                }
            }
        }
        out += "\n";
    }

    long long month_total = 0;
    int month_days = 0;
    for (int back = 0; back < 30; ++back)
    {
        const auto found = per_day.find(day_key(now - static_cast<long long>(back) * 86400));
        if (found != per_day.end() && found->second > 0)
        {
            month_total += found->second;
            ++month_days;
        }
    }
    out += "All books\n";
    out += "Today  " + format_duration(s.all_today) + "\n";
    out += "Last 7 days  " + format_duration(s.all_week) + "\n";
    out += "Last 30 days  " + format_duration(month_total) + " on " + std::to_string(month_days) + (month_days == 1 ? " day" : " days") + "\n";
    if (month_days > 0)
    {
        out += "Average on a reading day  " + format_duration(month_total / month_days) + "\n";
    }
    out += "Total  " + format_duration(s.all_total) + "\n";
    {
        std::string best_day;
        long long best = 0;
        for (const auto &day : per_day)
        {
            if (day.second > best)
            {
                best = day.second;
                best_day = day.first;
            }
        }
        if (best > 0)
        {
            out += "Best day  " + best_day + ", " + format_duration(best) + "\n";
        }
    }

    out += "\nLast 14 days\n";
    {
        long long biggest = 1;
        for (int back = 0; back < 14; ++back)
        {
            const auto found = per_day.find(day_key(now - static_cast<long long>(back) * 86400));
            if (found != per_day.end())
            {
                biggest = std::max(biggest, found->second);
            }
        }
        for (int back = 13; back >= 0; --back)
        {
            const long long when = now - static_cast<long long>(back) * 86400;
            const std::string key = day_key(when);
            const auto found = per_day.find(key);
            const long long secs = found == per_day.end() ? 0 : found->second;
            const int bar = static_cast<int>(secs * 16 / biggest);
            out += key.substr(5) + "  " + std::string(static_cast<size_t>(bar), '#') + (secs > 0 && bar == 0 ? "." : "")
                + (secs > 0 ? "  " + format_duration(secs) : std::string()) + "\n";
        }
    }

    {
        std::vector<std::pair<long long, std::string>> ranked;
        for (const auto &item : per_book)
        {
            ranked.emplace_back(item.second, item.first);
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        if (!ranked.empty())
        {
            out += "\nMost read books\n";
            for (size_t i = 0; i < ranked.size() && i < 8; ++i)
            {
                out += format_duration(ranked[i].first) + "  " + (title_for ? title_for(ranked[i].second) : ranked[i].second) + "\n";
            }
        }
    }

    {
        std::map<std::string, int> months;
        for (long long when : finished_times)
        {
            if (when > 0)
            {
                ++months[day_key(when).substr(0, 7)];
            }
        }
        if (!months.empty())
        {
            out += "\nBooks finished by month (when last opened)\n";
            int shown = 0;
            for (auto it = months.rbegin(); it != months.rend() && shown < 12; ++it, ++shown)
            {
                out += it->first + "  " + std::to_string(it->second) + "\n";
            }
        }
    }

    out += "\nBooks started  " + std::to_string(books_started) + "\n";
    out += "Books finished  " + std::to_string(books_finished);
    return out;
}

} // namespace reading_stats
