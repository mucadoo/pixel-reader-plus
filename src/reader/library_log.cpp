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
        // percent, time, status, favorite, book id, path, separated by tabs. Older
        // files lack the book id, or have only percent, time and path.
        std::vector<std::string> parts;
        size_t from = 0;
        while (true)
        {
            const size_t tab = line.find('\t', from);
            if (tab == std::string::npos)
            {
                parts.push_back(line.substr(from));
                break;
            }
            parts.push_back(line.substr(from, tab - from));
            from = tab + 1;
        }
        if (parts.size() != 3 && parts.size() < 5)
        {
            continue;
        }
        try
        {
            Entry entry;
            entry.percent = std::clamp(std::stoi(parts[0]), 0, 100);
            entry.last_opened = std::stoll(parts[1]);
            std::string path;
            if (parts.size() >= 5)
            {
                const int status = std::stoi(parts[2]);
                entry.status = status == 1 ? Status::Finished
                    : (status == 2 ? Status::Reading : (status == 3 ? Status::ToRead : (status == 4 ? Status::OnHold : Status::Auto)));
                entry.favorite = parts[3] == "1";
                size_t path_at = 4;
                if (parts.size() >= 6)
                {
                    entry.book_id = parts[4];
                    const size_t comma = entry.book_id.find(',');
                    if (comma != std::string::npos)
                    {
                        // the length of the book rides after the id
                        try { entry.units = static_cast<uint32_t>(std::stoul(entry.book_id.substr(comma + 1))); } catch (...) {}
                        entry.book_id.erase(comma);
                    }
                    path_at = 5;
                }
                for (size_t i = path_at; i < parts.size(); ++i)
                {
                    path += (i > path_at ? "\t" : "") + parts[i];
                }
            }
            else
            {
                path = parts[2];
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
            << (e.status == Status::Finished ? 1 : (e.status == Status::Reading ? 2 : (e.status == Status::ToRead ? 3 : (e.status == Status::OnHold ? 4 : 0)))) << '\t'
            << (e.favorite ? 1 : 0) << '\t' << e.book_id << (e.units ? "," + std::to_string(e.units) : std::string()) << '\t' << item.first << '\n';
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

void LibraryLog::set_book_id(const std::filesystem::path &book, const std::string &book_id)
{
    Entry &entry = entries[book.string()];
    if (entry.book_id != book_id)
    {
        entry.book_id = book_id;
        dirty = true;
    }
}

void LibraryLog::set_units(const std::filesystem::path &book, uint32_t units)
{
    Entry &entry = entries[book.string()];
    if (entry.units != units)
    {
        entry.units = units;
        dirty = true;
    }
}

std::vector<std::filesystem::path> LibraryLog::recent(size_t max_count) const
{
    std::vector<std::pair<long long, std::string>> found;
    for (const auto &item : entries)
    {
        if (item.second.last_opened > 0)
        {
            std::error_code ec;
            if (std::filesystem::exists(item.first, ec))
            {
                found.emplace_back(item.second.last_opened, item.first);
            }
        }
    }
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<std::filesystem::path> result;
    for (size_t i = 0; i < found.size() && i < max_count; ++i)
    {
        result.emplace_back(found[i].second);
    }
    return result;
}

void LibraryLog::remove(const std::filesystem::path &book)
{
    if (entries.erase(book.string()) > 0)
    {
        dirty = true;
    }
}

void LibraryLog::rename(const std::filesystem::path &from, const std::filesystem::path &to)
{
    const auto it = entries.find(from.string());
    if (it == entries.end())
    {
        return;
    }
    Entry entry = it->second;
    entries.erase(it);
    entries[to.string()] = entry;
    dirty = true;
}

int LibraryLog::started_count() const
{
    int count = 0;
    for (const auto &item : entries)
    {
        if (item.second.last_opened > 0)
        {
            ++count;
        }
    }
    return count;
}

std::vector<std::pair<std::filesystem::path, LibraryLog::Entry>> LibraryLog::all() const
{
    std::vector<std::pair<std::filesystem::path, Entry>> result;
    for (const auto &item : entries)
    {
        std::error_code ec;
        if (std::filesystem::exists(item.first, ec))
        {
            result.emplace_back(item.first, item.second);
        }
    }
    return result;
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