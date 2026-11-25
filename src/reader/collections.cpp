#include "./collections.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <system_error>

namespace collections
{

namespace
{

std::filesystem::path store_file;
std::map<std::string, std::vector<std::string>> shelves;
bool dirty = false;

} // namespace

void load(std::filesystem::path file)
{
    store_file = std::move(file);
    shelves.clear();
    std::ifstream in(store_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0)
        {
            continue;
        }
        const std::string name = line.substr(0, tab);
        const std::string path = line.substr(tab + 1);
        auto &list = shelves[name]; // a line with no path still makes the collection
        if (!path.empty() && std::find(list.begin(), list.end(), path) == list.end())
        {
            list.push_back(path);
        }
    }
    dirty = false;
}

void flush()
{
    if (!dirty || store_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(store_file.parent_path(), ec);
    std::ofstream out(store_file, std::ios::trunc);
    for (const auto &shelf : shelves)
    {
        if (shelf.second.empty())
        {
            out << shelf.first << '\t' << '\n';
        }
        for (const auto &path : shelf.second)
        {
            out << shelf.first << '\t' << path << '\n';
        }
    }
    dirty = false;
}

std::vector<std::string> names()
{
    std::vector<std::string> result;
    for (const auto &shelf : shelves)
    {
        result.push_back(shelf.first);
    }
    return result;
}

std::vector<std::filesystem::path> books(const std::string &name)
{
    std::vector<std::filesystem::path> result;
    const auto it = shelves.find(name);
    if (it == shelves.end())
    {
        return result;
    }
    for (const auto &path : it->second)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            result.emplace_back(path);
        }
    }
    return result;
}

int count(const std::string &name)
{
    return static_cast<int>(books(name).size());
}

bool contains(const std::string &name, const std::filesystem::path &book)
{
    const auto it = shelves.find(name);
    return it != shelves.end() && std::find(it->second.begin(), it->second.end(), book.string()) != it->second.end();
}

void add(const std::string &name, const std::filesystem::path &book)
{
    if (name.empty())
    {
        return;
    }
    auto &list = shelves[name];
    if (std::find(list.begin(), list.end(), book.string()) == list.end())
    {
        list.push_back(book.string());
    }
    dirty = true;
}

void remove(const std::string &name, const std::filesystem::path &book)
{
    const auto it = shelves.find(name);
    if (it == shelves.end())
    {
        return;
    }
    auto &list = it->second;
    list.erase(std::remove(list.begin(), list.end(), book.string()), list.end());
    dirty = true;
}

void create(const std::string &name)
{
    if (!name.empty() && !shelves.count(name))
    {
        shelves[name];
        dirty = true;
    }
}

void delete_collection(const std::string &name)
{
    if (shelves.erase(name) > 0)
    {
        dirty = true;
    }
}

void forget_book(const std::filesystem::path &book)
{
    for (auto &shelf : shelves)
    {
        auto &list = shelf.second;
        const size_t before = list.size();
        list.erase(std::remove(list.begin(), list.end(), book.string()), list.end());
        if (list.size() != before)
        {
            dirty = true;
        }
    }
}

void rename_book(const std::filesystem::path &from, const std::filesystem::path &to)
{
    for (auto &shelf : shelves)
    {
        for (auto &path : shelf.second)
        {
            if (path == from.string())
            {
                path = to.string();
                dirty = true;
            }
        }
    }
}

} // namespace collections
