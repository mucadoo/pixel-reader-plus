#include "./pending_progress.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

namespace pending_progress
{

namespace
{

std::filesystem::path progress_file;
std::map<std::string, int> places;

void save()
{
    if (progress_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(progress_file.parent_path(), ec);
    std::ofstream out(progress_file, std::ios::trunc);
    for (const auto &place : places)
    {
        out << place.second << '\t' << place.first << '\n';
    }
}

} // namespace

void load(std::filesystem::path file)
{
    progress_file = std::move(file);
    places.clear();
    std::ifstream in(progress_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab + 1 >= line.size())
        {
            continue;
        }
        try
        {
            places[line.substr(tab + 1)] = std::clamp(std::stoi(line.substr(0, tab)), 0, 100);
        }
        catch (...)
        {
        }
    }
}

void set(const std::filesystem::path &book, int percent)
{
    places[book.string()] = std::clamp(percent, 0, 100);
    save();
}

std::optional<int> take(const std::filesystem::path &book)
{
    const auto found = places.find(book.string());
    if (found == places.end())
    {
        return std::nullopt;
    }
    const int percent = found->second;
    places.erase(found);
    save();
    return percent;
}

} // namespace pending_progress
