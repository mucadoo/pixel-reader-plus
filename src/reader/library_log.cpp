#include "./library_log.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

LibraryLog &library_log()
{
    static LibraryLog log;
    return log;
}

void LibraryLog::load(std::filesystem::path path)
{
    file = std::move(path);
    entries.clear();
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // percent, tab, time, tab, path
        const size_t first = line.find('\t');
        const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos)
        {
            continue;
        }
        try
        {
            Entry entry;
            entry.percent = std::clamp(std::stoi(line.substr(0, first)), 0, 100);
            entry.last_opened = std::stoll(line.substr(first + 1, second - first - 1));
            entries[line.substr(second + 1)] = entry;
        }
        catch (...)
        {
        }
    }
    dirty = false;
}

void LibraryLog::flush()
{
    if (!dirty || file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    for (const auto &item : entries)
    {
        out << item.second.percent << '\t' << item.second.last_opened << '\t' << item.first << '\n';
    }
    dirty = false;
}

void LibraryLog::touch(const std::filesystem::path &book, long long now)
{
    Entry &entry = entries[book.string()];
    entry.last_opened = now;
    dirty = true;
}

void LibraryLog::set_percent(const std::filesystem::path &book, int percent)
{
    Entry &entry = entries[book.string()];
    percent = std::clamp(percent, 0, 100);
    if (entry.percent != percent)
    {
        entry.percent = percent;
        dirty = true;
    }
}

const LibraryLog::Entry *LibraryLog::get(const std::filesystem::path &book) const
{
    const auto it = entries.find(book.string());
    return it == entries.end() ? nullptr : &it->second;
}

std::optional<std::filesystem::path> LibraryLog::most_recent() const
{
    const std::string *best = nullptr;
    long long best_time = -1;
    for (const auto &item : entries)
    {
        if (item.second.last_opened > best_time)
        {
            std::error_code ec;
            if (std::filesystem::exists(item.first, ec))
            {
                best = &item.first;
                best_time = item.second.last_opened;
            }
        }
    }
    if (!best)
    {
        return std::nullopt;
    }
    return std::filesystem::path(*best);
}