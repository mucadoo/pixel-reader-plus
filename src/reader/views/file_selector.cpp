#include "./file_selector.h"

#include "./book_grid.h"
#include "./selection_menu.h"
#include "filetypes/open_doc.h"
#include "reader/book_covers.h"
#include "reader/system_styling.h"
#include "sys/filesystem.h"
#include "sys/keymap.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

struct FSState
{
    std::filesystem::path path;
    std::vector<FSEntry> path_entries;
    std::function<void(const std::filesystem::path &)> on_file_selected;
    std::function<void(const std::filesystem::path &)> on_file_focus;
    std::function<void()> on_view_focus;
    std::function<void(const std::string &, const std::string &)> on_info;
    std::function<void(bool)> on_view_mode_change;

    SelectionMenu menu;
    std::unique_ptr<BookCovers> covers;
    BookGrid grid;
    bool grid_mode = false;
    uint32_t cursor = 0; // the chosen entry, shared by the list and the grid

    FSState(std::filesystem::path path, SystemStyling &styling)
        : path(path),
          menu(styling),
          grid(styling)
    {
    }
};

namespace {

std::string format_size(unsigned long long bytes)
{
    char text[32];
    if (bytes < 1024)
    {
        std::snprintf(text, sizeof(text), "%llu B", bytes);
    }
    else if (bytes < 1024ull * 1024)
    {
        std::snprintf(text, sizeof(text), "%llu KB", (bytes + 512) / 1024);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return text;
}

std::string book_title(const std::string &file_name)
{
    std::string stem = std::filesystem::path(file_name).stem().string();
    return stem.empty() ? file_name : stem;
}

std::string book_kind(const std::string &file_name)
{
    std::string ext = std::filesystem::path(file_name).extension().string();
    if (!ext.empty() && ext[0] == '.')
    {
        ext.erase(0, 1);
    }
    for (auto &c : ext)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return ext;
}

std::string folder_title(const std::filesystem::path &path)
{
    std::string name = path.filename().string();
    return name.empty() ? "/" : name;
}

// Put the cursor on this entry in both the list and the grid.
void sync_cursor(FSState *s, uint32_t index)
{
    s->cursor = index;
    s->menu.set_cursor_pos(index);
    s->grid.set_cursor(index);
}

// Put the cursor on the entry with this name. False when there is none.
bool focus_entry(FSState *s, const std::string &name)
{
    for (size_t i = 0; i < s->path_entries.size(); ++i)
    {
        if (s->path_entries[i].name == name)
        {
            sync_cursor(s, static_cast<uint32_t>(i));
            return true;
        }
    }
    return false;
}

// Put the cursor on the first row that is not the way up.
void focus_first_item(FSState *s)
{
    for (size_t i = 0; i < s->path_entries.size(); ++i)
    {
        if (s->path_entries[i].name != "..")
        {
            sync_cursor(s, static_cast<uint32_t>(i));
            return;
        }
    }
    sync_cursor(s, 0u);
}

std::vector<std::string> hints_for(const FSState *s, bool way_up)
{
    std::vector<std::string> hints = {"A  Open", "Y  Info", s->grid_mode ? "Select  List" : "Select  Grid"};
    if (way_up)
    {
        hints.push_back("B  Back");
    }
    hints.push_back("Menu  Quit");
    return hints;
}

bool has_way_up(const FSState *s)
{
    return !s->path_entries.empty() && s->path_entries[0].is_dir && s->path_entries[0].name == "..";
}

void refresh_path_entries(FSState *s)
{
    s->path_entries.clear();
    if (s->path.has_parent_path() && s->path != s->path.root_path())
    {
        s->path_entries.push_back(FSEntry::directory(".."));
    }

    for (const auto &entry : directory_listing(s->path)) {
        if (entry.is_dir || file_type_is_supported(entry.name)) {
            s->path_entries.push_back(entry);
        }
    }

    std::vector<std::string> menu_entries;
    std::vector<std::string> menu_details;
    std::vector<BookGrid::Item> grid_items;
    bool has_items = false;
    for (const auto &entry : s->path_entries)
    {
        BookGrid::Item item;
        item.path = s->path / entry.name;

        if (entry.is_dir && entry.name == "..")
        {
            menu_entries.push_back("Up one folder");
            menu_details.push_back("");
            item.kind = BookGrid::Item::Kind::Up;
            item.label = "Up one folder";
            grid_items.push_back(item);
            continue;
        }

        has_items = true;
        if (entry.is_dir)
        {
            menu_entries.push_back(entry.name);
            menu_details.push_back("Folder");
            item.kind = BookGrid::Item::Kind::Folder;
            item.label = entry.name;
        }
        else
        {
            menu_entries.push_back(book_title(entry.name));
            std::string kind = book_kind(entry.name);
            menu_details.push_back(kind.empty() ? format_size(entry.size) : kind + ", " + format_size(entry.size));
            item.kind = BookGrid::Item::Kind::Book;
            item.label = book_title(entry.name);
            item.tag = kind;
        }
        grid_items.push_back(item);
    }

    const std::string title = folder_title(s->path);
    const auto hints = hints_for(s, has_way_up(s));
    const std::string empty_message = has_items ? "" : "No books in this folder\nAdd .epub or .txt files to Roms/EBOOKS";

    s->menu.set_title(title);
    s->menu.set_entries(menu_entries);
    s->menu.set_details(menu_details);
    s->menu.set_hints(hints);
    s->menu.set_empty_message(empty_message);

    s->grid.set_title(title);
    s->grid.set_items(std::move(grid_items));
    s->grid.set_hints(hints);
    s->grid.set_empty_message(empty_message);
    s->cursor = 0;
}

// Text for the properties screen of an entry.
std::string properties_text(const FSState *s, const FSEntry &entry)
{
    std::string body;
    body += "Name\n" + entry.name + "\n\n";
    if (entry.is_dir)
    {
        body += "Type\nFolder\n\n";
    }
    else
    {
        std::string kind = book_kind(entry.name);
        body += "Type\n" + (kind.empty() ? std::string("Book") : kind + " book") + "\n\n";
        body += "Size\n" + format_size(entry.size) + "\n\n";
    }
    body += "Location\n" + s->path.string();
    return body;
}

void show_info(FSState *s)
{
    if (!s->on_info || s->path_entries.empty() || s->cursor >= s->path_entries.size())
    {
        return;
    }
    const FSEntry &entry = s->path_entries[s->cursor];
    if (entry.is_dir && entry.name == "..")
    {
        return;
    }
    s->on_info("Properties", properties_text(s, entry));
}

// Go to the parent folder, with the cursor on the folder we came from.
void go_up(FSState *s)
{
    std::string highlight_name = s->path.filename().string();

    s->path = s->path.parent_path();
    refresh_path_entries(s);
    if (!focus_entry(s, highlight_name))
    {
        focus_first_item(s);
    }
}

void on_menu_entry_selected(FSState *s, uint32_t menu_index)
{
    if (s->path_entries.empty())
    {
        return;
    }

    const FSEntry &entry = s->path_entries[menu_index];
    if (entry.is_dir)
    {
        if (entry.name == "..")
        {
            go_up(s);
        }
        else
        {
            // Go down a directory
            s->path /= entry.name;
            refresh_path_entries(s);
            focus_first_item(s); // get past ".." entry
        }
    }
    else
    {
        if (s->on_file_selected)
        {
            s->on_file_selected(s->path / entry.name);
        }
    }
}

void on_menu_entry_focused(FSState *s, uint32_t menu_index)
{
    if (menu_index >= s->path_entries.size())
    {
        return;
    }
    s->cursor = menu_index;
    if (s->on_file_focus)
    {
        const auto &entry = s->path_entries[menu_index];
        s->on_file_focus(s->path / entry.name);
    }
}

std::filesystem::path sanitize_starting_path(std::filesystem::path path)
{
    path = std::filesystem::absolute(path);

    if (path.has_parent_path())
    {
        // get the directory component
        path = path.parent_path();
    }

    // make sure path exists
    while (!std::filesystem::is_directory(path))
    {
        std::cerr << "Directory " << path << " does not exist" << std::endl;
        if (path.has_parent_path() && path != path.root_path())
        {
            path = path.parent_path();
        }
        else
        {
            path = std::filesystem::current_path();
            break;
        }
    }

    return path;
}

} // namespace

FileSelector::FileSelector(std::filesystem::path path, SystemStyling &styling)
    : state(std::make_unique<FSState>(
          sanitize_starting_path(path),
          styling
      ))
{
    state->menu.set_on_selection([this](uint32_t menu_index) {
        on_menu_entry_selected(this->state.get(), menu_index);
    });

    state->menu.set_on_focus([this](uint32_t menu_index) {
        on_menu_entry_focused(this->state.get(), menu_index);
    });

    state->grid.set_on_focus([this](uint32_t index) {
        on_menu_entry_focused(this->state.get(), index);
    });

    state->grid.set_on_open([this](uint32_t index) {
        on_menu_entry_selected(this->state.get(), index);
    });

    refresh_path_entries(state.get());
    if (!path.has_filename() || !focus_entry(state.get(), path.filename().string()))
    {
        focus_first_item(state.get()); // get past ".." entry
    }
}

FileSelector::~FileSelector()
{
}

bool FileSelector::render(SDL_Surface *dest_surface, bool force_render)
{
    if (state->grid_mode)
    {
        return state->grid.render(dest_surface, force_render);
    }
    return state->menu.render(dest_surface, force_render);
}

bool FileSelector::tick(uint32_t elapsed_ms)
{
    if (state->grid_mode)
    {
        return state->grid.tick(elapsed_ms);
    }
    return state->menu.tick(elapsed_ms);
}

bool FileSelector::is_done()
{
    return state->menu.is_done();
}

void FileSelector::on_keypress(SDLKey key)
{
    if (key == SW_BTN_B)
    {
        // B walks back up the folder tree. Quitting is the Menu button's job.
        if (has_way_up(state.get()))
        {
            go_up(state.get());
        }
        return;
    }
    if (key == SW_BTN_Y)
    {
        show_info(state.get());
        return;
    }
    if (key == SW_BTN_SELECT)
    {
        set_grid_view(!state->grid_mode);
        return;
    }
    if (state->grid_mode)
    {
        state->grid.on_keypress(key);
    }
    else
    {
        state->menu.on_keypress(key);
    }
}

void FileSelector::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    if (state->grid_mode)
    {
        state->grid.on_keyheld(key, held_time_ms);
    }
    else
    {
        state->menu.on_keyheld(key, held_time_ms);
    }
}

void FileSelector::set_grid_view(bool grid)
{
    if (grid == state->grid_mode)
    {
        return;
    }
    state->grid_mode = grid;
    const auto hints = hints_for(state.get(), has_way_up(state.get()));
    state->menu.set_hints(hints);
    state->grid.set_hints(hints);
    if (!state->path_entries.empty())
    {
        sync_cursor(state.get(), std::min<uint32_t>(state->cursor, static_cast<uint32_t>(state->path_entries.size()) - 1));
    }
    state->grid.invalidate();
    if (state->on_view_mode_change)
    {
        state->on_view_mode_change(grid);
    }
}

bool FileSelector::grid_view() const
{
    return state->grid_mode;
}

void FileSelector::set_on_view_mode_change(std::function<void(bool)> callback)
{
    state->on_view_mode_change = callback;
}

void FileSelector::set_on_info(std::function<void(const std::string &, const std::string &)> callback)
{
    state->on_info = callback;
}

void FileSelector::set_cover_cache_dir(std::filesystem::path dir)
{
    state->covers = std::make_unique<BookCovers>(dir);
    state->grid.set_covers(state->covers.get());
}

void FileSelector::on_focus()
{
    if (state->on_view_focus)
    {
        state->on_view_focus();
    }
}

void FileSelector::set_on_file_selected(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_selected = callback;
}

void FileSelector::set_on_file_focus(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_focus = callback;
}

void FileSelector::set_on_view_focus(std::function<void()> callback)
{
    state->on_view_focus = callback;
}
