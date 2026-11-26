#include "reader/folder_prefs.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace folder_prefs
{

namespace
{

std::filesystem::path file_path;
std::map<std::string, Prefs> known;
bool loaded = false;

void load()
{
    if (loaded)
    {
        return;
    }
    loaded = true;
    known.clear();
    std::ifstream in(file_path);
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream fields(line);
        std::string folder, sort, grid, hide;
        if (!std::getline(fields, folder, '\t') || !std::getline(fields, sort, '\t') || !std::getline(fields, grid, '\t'))
        {
            continue;
        }
        std::getline(fields, hide, '\t');
        Prefs prefs;
        prefs.sort = sort.empty() ? -1 : std::atoi(sort.c_str());
        prefs.grid = grid.empty() ? -1 : std::atoi(grid.c_str());
        prefs.hide = hide.empty() ? -1 : std::atoi(hide.c_str());
        if (!folder.empty() && !prefs.empty())
        {
            known[folder] = prefs;
        }
    }
}

void save()
{
    if (file_path.empty())
    {
        return;
    }
    std::ofstream out(file_path, std::ios::trunc);
    for (const auto &entry : known)
    {
        out << entry.first << '\t'
            << (entry.second.sort < 0 ? std::string() : std::to_string(entry.second.sort)) << '\t'
            << (entry.second.grid < 0 ? std::string() : std::to_string(entry.second.grid)) << '\t'
            << (entry.second.hide < 0 ? std::string() : std::to_string(entry.second.hide)) << '\n';
    }
}

} // namespace

void set_file(const std::filesystem::path &file)
{
    file_path = file;
    loaded = false;
}

std::optional<Prefs> get(const std::string &folder)
{
    load();
    const auto it = known.find(folder);
    if (it == known.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void update(const std::string &folder, const Prefs &changes)
{
    load();
    Prefs &prefs = known[folder];
    if (changes.sort >= 0) prefs.sort = changes.sort;
    if (changes.grid >= 0) prefs.grid = changes.grid;
    if (changes.hide >= 0) prefs.hide = changes.hide;
    save();
}

void forget(const std::string &folder)
{
    load();
    if (known.erase(folder))
    {
        save();
    }
}

} // namespace folder_prefs