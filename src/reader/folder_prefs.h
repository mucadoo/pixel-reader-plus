#ifndef FOLDER_PREFS_H_
#define FOLDER_PREFS_H_

#include <filesystem>
#include <optional>
#include <string>

// How the book list looks in one folder: its order, list or grid, and whether finished books show.
// A field of -1 means the folder has no choice of its own for it.
namespace folder_prefs
{

struct Prefs
{
    int sort = -1;   // a BrowserSort
    int grid = -1;   // 0 list, 1 grid
    int hide = -1;   // 0 show finished books, 1 hide them

    bool empty() const { return sort < 0 && grid < 0 && hide < 0; }
};

void set_file(const std::filesystem::path &file);
// What the folder chose, if anything.
std::optional<Prefs> get(const std::string &folder);
// Adds the fields of `changes` that are set to what the folder has.
void update(const std::string &folder, const Prefs &changes);
void forget(const std::string &folder);

} // namespace folder_prefs

#endif