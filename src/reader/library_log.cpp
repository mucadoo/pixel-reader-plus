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
        // percent, time, status, favorite, path, separated by tabs. Older files
        // have only percent, time and path.
        std::vector<std::string> parts;
        size_t from = 0;
        for (int field = 0; field < 4; ++field)
        {
            const size_t tab = line.find('\t', from);
            if (tab == std::string::npos)
            {
                break;
            }
            parts.push_back(line.substr(from, tab - from));
            from = tab + 1;
        }
        const bool current = parts.size() == 4;
        if (!current && parts.size() < 2)
        {
            continue;
        }
        try
        {
            Entry entry;
            entry.percent = std::clamp(std::stoi(parts[0]), 0, 100);
            entry.last_opened = std::stoll(parts[1]);
            std::string path;
            if (current)
            {
                const int status = std::stoi(parts[2]);
                entry.status = status == 1 ? Status::Finished : (status == 2 ? Status::Reading : Status::Auto);
                entry.favorite = parts[3] == "1";
                path = line.substr(from);
            }
            else
            {
                path = line.substr(parts[0].size() + 1 + parts[1].size() + 1);
            }
            entries[path] = entry;
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
        const Entry &e = item.second;
        out << e.percent << '\t' << e.last_opened << '\t'
            << (e.status == Status::Finished ? 1 : (e.status == Status::Reading ? 2 : 0)) << '\t'
            << (e.favorite ? 1 : 0) << '\t' << item.first << '\n';
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

void LibraryLog::set_status(const std::filesystem::path &book, Status status)
{
    Entry &entry = entries[book.string()];
    if (entry.status != status)
    {
        entry.status = status;
        dirty = true;
    }
}

void LibraryLog::set_favorite(const std::filesystem::path &book, bool favorite)
{
    Entry &entry = entries[book.string()];
    if (entry.favorite != favorite)
    {
        entry.favorite = favorite;
        dirty = true;
    }
}

int LibraryLog::started_count() const
{
    return static_cast<int>(entries.size());
}

int LibraryLog::finished_count() const
{
    int count = 0;
    for (const auto &item : entries)
    {
        if (item.second.finished())
        {
            ++count;
        }
    }
    return count;
}

std::vector<std::filesystem::path> LibraryLog::favorites() const
{
    std::vector<std::filesystem::path> result;
    for (const auto &item : entries)
    {
        if (!item.second.favorite)
        {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::exists(item.first, ec))
        {
            result.emplace_back(item.first);
        }
    }
    return result;
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