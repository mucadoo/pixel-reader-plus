#include "./file_selector.h"

#include "./book_grid.h"
#include "./selection_menu.h"
#include "doc_api/doc_reader.h"
#include "filetypes/open_doc.h"
#include "reader/book_search.h"
#include "reader/folder_prefs.h"
#include "reader/highlights.h"
#include "reader/text_style.h"
#include "reader/next_book.h"
#include "reader/backup.h"
#include "reader/book_covers.h"
#include "reader/book_info.h"
#include "reader/browser_sort.h"
#include "reader/collections.h"
#include "reader/library_log.h"
#include "reader/opds.h"
#include "./book_info_view.h"
#include "./upload_view.h"
#include "./reading_history_view.h"
#include "reader/progress_screen.h"
#include "reader/reading_speed.h"
#include "reader/reading_stats.h"
#include "./text_input_view.h"
#include "reader/system_styling.h"
#include "sys/filesystem.h"
#include "sys/keymap.h"

#include <algorithm>
#include <ctime>
#include <cctype>
#include <cstring>
#include <sys/stat.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <SDL/SDL.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <vector>

struct FSState
{
    std::filesystem::path path;
    std::filesystem::path top_folder; // empty when the browser may go all the way up
    std::vector<FSEntry> path_entries;
    std::function<void(const std::filesystem::path &)> on_file_selected;
    std::function<void(const std::filesystem::path &)> on_file_focus;
    std::function<void()> on_view_focus;
    std::function<void(const std::string &, const std::string &)> on_info;
    std::function<void(bool)> on_view_mode_change;
    std::function<void(BrowserSort)> on_sort_change;
    std::function<void(const std::filesystem::path &, DocAddr)> on_open_at;
    BrowserSort sort = BrowserSort::Name;
    std::string series_next_for; // the finished book the next volume below was worked out for
    std::string series_next; // the next volume of its series, empty when there is none
    std::string continue_name; // name of the Continue reading row, empty when there is none
    size_t pinned_count = 0; // rows after the way up that are not part of the folder: Continue and favorites
    std::map<std::string, std::string> pinned_prefix; // name of a pinned row to the text put in front of it
    bool hide_finished = false;
    int filter = 0; // which books the folder lists: 0 all, 1 not opened, 2 in progress, 3 favorites, 4 finished
    // What folders without a choice of their own use, and the folder the view was last set up for.
    BrowserSort global_sort = BrowserSort::Name;
    bool global_grid = false;
    bool global_hide = false;
    std::string prefs_for;
    SystemStyling &styling;
    std::function<void(std::shared_ptr<View>)> on_open_view;
    std::function<void(bool)> on_hide_finished_change;
    std::function<void(const std::string &)> on_book_deleted;
    std::filesystem::path store_dir;     // where the reader keeps its files, for backups
    std::function<void()> flush_state;   // writes the settings and book places to disk

    bool search_active = false; // the list shows results: a search, or the books of a collection
    std::string results_title;  // heading of those results
    std::string results_up = "Clear search";
    std::string collection;     // the collection being browsed, empty for a search
    std::string search_query;
    std::vector<std::filesystem::path> search_hits;
    std::set<std::string> marked;                 // books marked for copy, move or delete (whole paths)
    std::vector<std::filesystem::path> clipboard; // books copied or cut, waiting for Paste here
    bool clipboard_move = false;

    SelectionMenu menu;
    std::unique_ptr<BookCovers> covers;
    std::unique_ptr<BookInfoCache> info;
    BookGrid grid;
    bool grid_mode = false;
    uint32_t cursor = 0; // the chosen entry, shared by the list and the grid

    FSState(std::filesystem::path path, SystemStyling &styling)
        : path(path),
          styling(styling),
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

// How long the book takes at your reading speed: what is left for a book you have
// opened, a rough whole-book figure for text formats, nothing for an unopened epub.
std::string reading_time_text(const FSEntry &entry, const LibraryLog::Entry *log)
{
    long long seconds = -1;
    bool rough = false;
    if (log && log->units > 0)
    {
        if (log->finished())
        {
            return "";
        }
        seconds = reading_speed::seconds_for(static_cast<uint32_t>(static_cast<unsigned long long>(log->units) * (100 - log->percent) / 100));
    }
    else
    {
        const std::string kind = book_kind(entry.name);
        if (kind == "EPUB" || entry.size == 0)
        {
            return "";
        }
        seconds = reading_speed::seconds_for(static_cast<uint32_t>(entry.size * 85 / 100));
        rough = true;
    }
    if (seconds < 0)
    {
        return "";
    }
    return (rough ? "~" : "") + reading_stats::format_duration(seconds) + (rough ? "" : " left");
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
    std::vector<std::string> hints = {"A  Open", "Y  Options", s->grid_mode ? "Select  List" : "Select  Grid", "Start  Sort"};
    if (way_up)
    {
        hints.push_back("B  Back");
    }
    hints.push_back("Menu  Quit");
    return hints;
}

const char *filter_label(int filter)
{
    switch (filter)
    {
        case 1: return "not opened";
        case 2: return "in progress";
        case 3: return "favorites";
        case 4: return "finished";
        default: return "";
    }
}

// Whether a book is listed under the filter that is on.
bool passes_filter(int filter, const LibraryLog::Entry *log)
{
    switch (filter)
    {
        case 1: return !log;
        case 2: return log && !log->finished() && log->percent > 0;
        case 3: return log && log->favorite;
        case 4: return log && log->finished();
        default: return true;
    }
}

bool has_way_up(const FSState *s)
{
    return !s->path_entries.empty() && s->path_entries[0].is_dir && s->path_entries[0].name == "..";
}

void rebuild_views(FSState *s);
std::string entry_title(const FSState *s, const FSEntry &entry);

std::string lowercase(std::string text)
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// Order the books as the sort setting asks. The way up, the Continue reading
// row and the folders stay where they are.
void apply_sort(FSState *s)
{
    auto first = s->path_entries.begin();
    if (first != s->path_entries.end() && first->name == "..")
    {
        ++first;
    }
    first += static_cast<std::ptrdiff_t>(std::min<size_t>(s->pinned_count, static_cast<size_t>(s->path_entries.end() - first)));
    auto books = std::find_if(first, s->path_entries.end(), [](const FSEntry &entry) {
        return !entry.is_dir;
    });
    if (s->sort == BrowserSort::Name || books == s->path_entries.end())
    {
        return;
    }

    auto author_of = [s](const FSEntry &entry) {
        const BookInfo *info = s->info ? s->info->get(s->path / entry.name) : nullptr;
        return info ? lowercase(info->author) : std::string();
    };
    auto opened_at = [s](const FSEntry &entry) {
        const LibraryLog::Entry *log = library_log().get(s->path / entry.name);
        return log ? log->last_opened : 0LL;
    };
    // The date a file was put on the card is looked up once for each book.
    std::map<std::string, long long> added;
    if (s->sort == BrowserSort::Added)
    {
        for (auto it = books; it != s->path_entries.end(); ++it)
        {
            std::error_code ec;
            const auto when = std::filesystem::last_write_time(s->path / it->name, ec);
            added[it->name] = ec ? 0LL : static_cast<long long>(when.time_since_epoch().count());
        }
    }
    // 0 for a book in progress, 1 for one not opened, 2 for a finished one.
    auto progress_of = [s](const FSEntry &entry, int &percent) {
        const LibraryLog::Entry *log = library_log().get(s->path / entry.name);
        percent = log ? log->percent : 0;
        if (!log)
        {
            return 1;
        }
        return log->finished() ? 2 : (log->percent > 0 ? 0 : 1);
    };
    std::stable_sort(books, s->path_entries.end(), [&](const FSEntry &a, const FSEntry &b) {
        switch (s->sort)
        {
            case BrowserSort::Size:
                return a.size > b.size;
            case BrowserSort::Added:
                return added[a.name] > added[b.name];
            case BrowserSort::Progress:
            {
                int percent_a = 0;
                int percent_b = 0;
                const int group_a = progress_of(a, percent_a);
                const int group_b = progress_of(b, percent_b);
                if (group_a != group_b)
                {
                    return group_a < group_b;
                }
                return group_a == 0 && percent_a > percent_b;
            }
            case BrowserSort::Title:
                return lowercase(entry_title(s, a)) < lowercase(entry_title(s, b));
            case BrowserSort::Author:
            {
                const std::string author_a = author_of(a);
                const std::string author_b = author_of(b);
                if (author_a.empty() != author_b.empty())
                {
                    return !author_a.empty(); // books without an author last
                }
                if (author_a != author_b)
                {
                    return author_a < author_b;
                }
                return lowercase(entry_title(s, a)) < lowercase(entry_title(s, b));
            }
            case BrowserSort::Series:
            {
                const BookInfo *info_a = s->info ? s->info->get(s->path / a.name) : nullptr;
                const BookInfo *info_b = s->info ? s->info->get(s->path / b.name) : nullptr;
                const std::string series_a = info_a ? lowercase(info_a->series) : std::string();
                const std::string series_b = info_b ? lowercase(info_b->series) : std::string();
                if (series_a.empty() != series_b.empty())
                {
                    return !series_a.empty(); // books in no series last
                }
                if (series_a != series_b)
                {
                    return series_a < series_b;
                }
                const double number_a = info_a ? info_a->series_number() : 0.0;
                const double number_b = info_b ? info_b->series_number() : 0.0;
                if (number_a != number_b)
                {
                    return number_a < number_b;
                }
                return lowercase(entry_title(s, a)) < lowercase(entry_title(s, b));
            }
            case BrowserSort::Recent:
                return opened_at(a) > opened_at(b);
            case BrowserSort::Name:
            default:
                return false;
        }
    });
}

// A folder with its own order, view or finished setting gets them when it is entered.
void apply_folder_prefs(FSState *s)
{
    if (s->prefs_for == s->path.string() || (!s->prefs_for.empty() && s->prefs_for + "/" == s->path.string()))
    {
        return;
    }
    s->prefs_for = s->path.string();
    while (s->prefs_for.size() > 1 && s->prefs_for.back() == '/')
    {
        s->prefs_for.pop_back();
    }
    const auto prefs = folder_prefs::get(s->prefs_for);
    s->sort = prefs && prefs->sort >= 0 && prefs->sort <= static_cast<int>(BrowserSort::Progress) ? static_cast<BrowserSort>(prefs->sort) : s->global_sort;
    s->hide_finished = prefs && prefs->hide >= 0 ? prefs->hide != 0 : s->global_hide;
    const bool grid = prefs && prefs->grid >= 0 ? prefs->grid != 0 : s->global_grid;
    if (grid != s->grid_mode)
    {
        s->grid_mode = grid;
        const auto hints = hints_for(s, has_way_up(s));
        s->menu.set_hints(hints);
        s->grid.set_hints(hints);
        s->grid.invalidate();
    }
}

// The user changed the order, view or finished setting: the top folder sets the
// default for the others, any other folder keeps the change for itself.
void note_view_choice(FSState *s, int sort, int grid, int hide)
{
    if (s->search_active)
    {
        return;
    }
    const auto plain = [](std::string text) {
        while (text.size() > 1 && text.back() == '/')
        {
            text.pop_back();
        }
        return text;
    };
    if (plain(s->path.string()) == plain(s->top_folder.string()) || s->path == s->path.root_path())
    {
        if (sort >= 0)
        {
            s->global_sort = static_cast<BrowserSort>(sort);
            if (s->on_sort_change) s->on_sort_change(s->global_sort);
        }
        if (grid >= 0)
        {
            s->global_grid = grid != 0;
            if (s->on_view_mode_change) s->on_view_mode_change(s->global_grid);
        }
        if (hide >= 0)
        {
            s->global_hide = hide != 0;
            if (s->on_hide_finished_change) s->on_hide_finished_change(s->global_hide);
        }
        folder_prefs::forget(s->prefs_for);
        return;
    }
    folder_prefs::update(s->prefs_for, {sort, grid, hide});
}

void refresh_path_entries(FSState *s)
{
    apply_folder_prefs(s);
    s->path_entries.clear();
    s->continue_name.clear();
    if (s->search_active)
    {
        // The way out of the search, then the books found. Their names are whole paths.
        s->path_entries.push_back(FSEntry::directory(".."));
        s->pinned_count = s->search_hits.size();
        s->pinned_prefix.clear();
        for (const auto &hit : s->search_hits)
        {
            std::error_code ec;
            const auto size = std::filesystem::file_size(hit, ec);
            s->path_entries.push_back({hit.string(), false, ec ? 0ull : static_cast<unsigned long long>(size)});
        }
        rebuild_views(s);
        return;
    }
    if (s->path.has_parent_path() && s->path != s->path.root_path() && s->path != s->top_folder)
    {
        s->path_entries.push_back(FSEntry::directory(".."));
    }

    // The book read last, and the favorites, one press away from any folder
    // they are not in.
    s->pinned_count = 0;
    s->pinned_prefix.clear();
    const auto pin = [s](const std::filesystem::path &book, const std::string &prefix) {
        if (book.parent_path() == s->path || !file_type_is_supported(book.filename().string()))
        {
            return;
        }
        if (s->pinned_prefix.count(book.string()))
        {
            return;
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(book, ec);
        s->pinned_prefix[book.string()] = prefix;
        s->path_entries.push_back({book.string(), false, ec ? 0ull : static_cast<unsigned long long>(size)});
        ++s->pinned_count;
    };
    if (const auto recent = library_log().most_recent())
    {
        const LibraryLog::Entry *log = library_log().get(*recent);
        if (!(s->hide_finished && log && log->finished()))
        {
            pin(*recent, "Continue: ");
            if (s->pinned_prefix.count(recent->string()))
            {
                s->continue_name = recent->string();
            }
        }
    }
    if (const auto recent = library_log().most_recent())
    {
        const LibraryLog::Entry *log = library_log().get(*recent);
        if (log && log->finished() && s->info && std::filesystem::exists(*recent))
        {
            // The next volume of the series the finished book belongs to.
            if (s->series_next_for != recent->string())
            {
                s->series_next_for = recent->string();
                s->series_next.clear();
                if (auto next = next_book::find_next(*recent, next_book::shared_cache(), true))
                {
                    s->series_next = next->path.string();
                }
            }
            if (!s->series_next.empty() && !(s->hide_finished && library_log().get(s->series_next) && library_log().get(s->series_next)->finished()))
            {
                pin(s->series_next, "Next in series: ");
            }
        }
    }
    for (const auto &favorite : library_log().favorites())
    {
        pin(favorite, "* ");
    }
    for (const auto &entry : directory_listing(s->path)) {
        if (entry.is_dir) {
            s->path_entries.push_back(entry);
        } else if (file_type_is_supported(entry.name)) {
            const LibraryLog::Entry *log = library_log().get(s->path / entry.name);
            if (s->hide_finished && s->filter != 4 && log && log->finished() && !log->favorite) {
                continue;
            }
            if (!passes_filter(s->filter, log)) {
                continue;
            }
            s->path_entries.push_back(entry);
        }
    }

    apply_sort(s);
    rebuild_views(s);
}

// The epub's own title when it is known, else the file name without its extension.
std::string entry_title(const FSState *s, const FSEntry &entry)
{
    const BookInfo *info = s->info ? s->info->get(s->path / entry.name) : nullptr;
    if (info && !info->title.empty())
    {
        return info->title;
    }
    return book_title(entry.name);
}

// Fill the list and the grid from path_entries, and put the cursor on the first row.
void rebuild_views(FSState *s)
{
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
            const char *up_label = s->search_active ? s->results_up.c_str() : "Up one folder";
            menu_entries.push_back(up_label);
            menu_details.push_back("");
            item.kind = BookGrid::Item::Kind::Up;
            item.label = up_label;
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
            const LibraryLog::Entry *log = library_log().get(s->path / entry.name);
            const auto pinned = s->pinned_prefix.find(entry.name);
            std::string prefix = pinned != s->pinned_prefix.end() ? pinned->second : std::string();
            if (prefix.empty() && log && log->favorite)
            {
                prefix = "* ";
            }
            if (s->marked.count((s->path / entry.name).string()))
            {
                prefix = "[x] " + prefix;
            }
            const std::string title = entry_title(s, entry);
            const BookInfo *info = s->info ? s->info->get(s->path / entry.name) : nullptr;
            menu_entries.push_back(prefix + (info && !info->author.empty() ? title + " - " + info->author : title));
            std::string kind = book_kind(entry.name);
            std::string detail = kind.empty() ? format_size(entry.size) : kind + ", " + format_size(entry.size);
            const std::string time_left = reading_time_text(entry, log);
            if (!time_left.empty())
            {
                detail = time_left + ", " + detail;
            }
            if (log)
            {
                detail = (log->status == LibraryLog::Status::ToRead ? std::string("To read")
                          : log->status == LibraryLog::Status::OnHold ? "On hold " + std::to_string(log->percent) + "%"
                          : log->finished() ? std::string("Done") : std::to_string(log->percent) + "%") + ", " + detail;
            }
            menu_details.push_back(detail);
            item.kind = BookGrid::Item::Kind::Book;
            item.label = prefix + title;
            item.tag = kind;
            item.progress = log ? (log->finished() ? 100 : log->percent) : -1;
            item.finished = log && log->finished();
            item.badge = !log ? std::string()
                : log->status == LibraryLog::Status::ToRead ? std::string("To read")
                : log->status == LibraryLog::Status::OnHold ? std::string("On hold")
                : log->finished() ? std::string("Done") : std::string();
        }
        grid_items.push_back(item);
    }

    std::string folder = s->search_active
        ? s->results_title + "  (" + std::to_string(s->search_hits.size()) + ")"
        : folder_title(s->path);
    if (!s->search_active && *browser_sort_suffix(s->sort) != 0)
    {
        folder += std::string("  (") + browser_sort_suffix(s->sort) + ")";
    }
    if (!s->search_active && s->filter != 0)
    {
        folder += std::string("  [") + filter_label(s->filter) + "]";
    }
    const auto hints = hints_for(s, has_way_up(s));
    const std::string empty_message = has_items
        ? ""
        : (!s->search_active && s->filter != 0 ? std::string("No book here is ") + filter_label(s->filter) + "\nY, Show: switches the books shown" : s->search_active ? (s->collection.empty() ? "No book matches" : "This collection has no books") : "No books in this folder\nAdd .epub or .txt files to Roms/EBOOKS");

    s->menu.set_title(folder);
    s->menu.set_entries(menu_entries);
    s->menu.set_details(menu_details);
    s->menu.set_hints(hints);
    s->menu.set_empty_message(empty_message);

    s->grid.set_title(folder);
    s->grid.set_items(std::move(grid_items));
    s->grid.set_hints(hints);
    s->grid.set_empty_message(empty_message);
    s->cursor = 0;
}

// Sort again and rebuild the rows, with the cursor staying on the same entry.
void resort_keeping_cursor(FSState *s)
{
    const uint32_t keep = s->cursor;
    const std::string name = keep < s->path_entries.size() ? s->path_entries[keep].name : std::string();
    apply_sort(s);
    rebuild_views(s);
    if (s->path_entries.empty())
    {
        return;
    }
    if (name.empty() || !focus_entry(s, name))
    {
        sync_cursor(s, std::min<uint32_t>(keep, static_cast<uint32_t>(s->path_entries.size()) - 1));
    }
}

// Read a few more epubs for their titles and show them. True when something changed.
bool load_book_info(FSState *s)
{
    if (!s->info)
    {
        return false;
    }
    int budget = 3;
    bool changed = false;
    for (const auto &entry : s->path_entries)
    {
        if (budget <= 0)
        {
            break;
        }
        const std::string kind = book_kind(entry.name);
        if (entry.is_dir || (kind != "EPUB" && kind != "MD" && kind != "MARKDOWN"))
        {
            continue;
        }
        const std::filesystem::path book = s->path / entry.name;
        if (s->info->has(book))
        {
            continue;
        }
        s->info->load(book);
        --budget;
        changed = true;
    }
    if (changed)
    {
        resort_keeping_cursor(s);
    }
    return changed;
}

// Text for the properties screen of an entry.
std::string properties_text(const FSState *s, const FSEntry &entry)
{
    std::string body;
    body += "Name\n" + entry.name + "\n\n";
    if (!entry.is_dir && s->info)
    {
        if (const BookInfo *info = s->info->get(s->path / entry.name))
        {
            if (!info->title.empty())
            {
                body += "Title\n" + info->title + "\n\n";
            }
            if (!info->author.empty())
            {
                body += "Author\n" + info->author + "\n\n";
            }
            if (!info->series.empty())
            {
                body += "Series\n" + info->series + (info->series_index.empty() ? std::string() : " #" + info->series_index) + "\n\n";
            }
            if (!info->language.empty())
            {
                body += "Language\n" + info->language + "\n\n";
            }
        }
    }
    if (entry.is_dir)
    {
        body += "Type\nFolder\n\n";
    }
    else
    {
        std::string kind = book_kind(entry.name);
        body += "Type\n" + (kind.empty() ? std::string("Book") : kind + " book") + "\n\n";
        body += "Size\n" + format_size(entry.size) + "\n\n";
        const std::string time_left = reading_time_text(entry, library_log().get(s->path / entry.name));
        if (!time_left.empty())
        {
            body += "Reading time\n" + time_left + "\n\n";
        }
    }
    body += "Location\n" + s->path.string();
    return body;
}

void show_info(FSState *s);

// Read the folder again, with the cursor staying on the same entry when it is still listed.
void refresh_keeping_cursor(FSState *s)
{
    const uint32_t keep = s->cursor;
    const std::string name = keep < s->path_entries.size() ? s->path_entries[keep].name : std::string();
    refresh_path_entries(s);
    if (s->path_entries.empty())
    {
        return;
    }
    if (name.empty() || !focus_entry(s, name))
    {
        sync_cursor(s, std::min<uint32_t>(keep, static_cast<uint32_t>(s->path_entries.size()) - 1));
    }
}

// Looks through every book under the top folder for the words typed, by file name,
// title, author and series, and lists the matches.
void run_search(FSState *s, const std::string &query)
{
    std::vector<std::string> words;
    {
        std::string word;
        for (char c : lowercase(query) + " ")
        {
            if (c == ' ')
            {
                if (!word.empty())
                {
                    words.push_back(word);
                    word.clear();
                }
            }
            else
            {
                word.push_back(c);
            }
        }
    }
    if (words.empty())
    {
        return;
    }

    const std::filesystem::path root = s->top_folder.empty() ? s->path : s->top_folder;
    std::vector<std::filesystem::path> files;
    {
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            std::error_code ec_file;
            if (it->is_regular_file(ec_file) && file_type_is_supported(it->path().filename().string()))
            {
                files.push_back(it->path());
            }
        }
    }

    constexpr size_t MAX_HITS = 500;
    std::vector<std::pair<std::string, std::filesystem::path>> hits; // sort key and path
    for (size_t i = 0; i < files.size() && hits.size() < MAX_HITS; ++i)
    {
        progress_screen::show(s->styling, "Searching library", query, static_cast<int>(i * 100 / files.size()));
        const std::filesystem::path &book = files[i];
        std::string text = book.stem().string();
        std::string title = text;
        if (s->info)
        {
            s->info->load(book);
            if (const BookInfo *info = s->info->get(book))
            {
                text += " " + info->title + " " + info->author + " " + info->series;
                if (!info->title.empty())
                {
                    title = info->title;
                }
            }
        }
        text = lowercase(text);
        bool all = true;
        for (const auto &word : words)
        {
            if (text.find(word) == std::string::npos)
            {
                all = false;
                break;
            }
        }
        if (all)
        {
            hits.emplace_back(lowercase(title), book);
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    s->search_query = query;
    s->results_title = "Search: " + query;
    s->results_up = "Clear search";
    s->collection.clear();
    s->search_hits.clear();
    for (const auto &hit : hits)
    {
        s->search_hits.push_back(hit.second);
    }
    s->search_active = true;
    refresh_path_entries(s);
    focus_first_item(s);
}

// Renames the book file, keeping its extension. What is known about the book follows it.
void rename_book(FSState *s, const std::filesystem::path &book, const std::string &new_stem)
{
    std::string stem = new_stem;
    while (!stem.empty() && stem.back() == ' ')
    {
        stem.pop_back();
    }
    if (stem.empty() || stem == book.stem().string())
    {
        return;
    }
    if (stem.find_first_of("/\\:*?\"<>|") != std::string::npos || stem[0] == '.')
    {
        if (s->on_info)
        {
            s->on_info("Rename", "A file name cannot be empty, start with a dot, or contain / \\ : * ? \" < > |");
        }
        return;
    }
    const std::filesystem::path target = book.parent_path() / (stem + book.extension().string());
    std::error_code ec;
    if (std::filesystem::exists(target, ec))
    {
        if (s->on_info)
        {
            s->on_info("Rename", "A file with that name already exists.");
        }
        return;
    }
    std::filesystem::rename(book, target, ec);
    if (ec)
    {
        if (s->on_info)
        {
            s->on_info("Rename", "The file could not be renamed.");
        }
        return;
    }
    library_log().rename(book, target);
    library_log().flush();
    collections::rename_book(book, target);
    collections::flush();
    for (auto &hit : s->search_hits)
    {
        if (hit == book)
        {
            hit = target;
        }
    }
    refresh_path_entries(s);
    if (!focus_entry(s, s->search_active ? target.string() : target.filename().string()) && !focus_entry(s, target.string()))
    {
        focus_first_item(s);
    }
}

// Deletes the book file and everything the reader kept for it.
void delete_book(FSState *s, const std::filesystem::path &book)
{
    const LibraryLog::Entry *log = library_log().get(book);
    const std::string book_id = log ? log->book_id : std::string();
    std::error_code ec;
    std::filesystem::remove(book, ec);
    if (ec)
    {
        if (s->on_info)
        {
            s->on_info("Delete", "The book could not be deleted.");
        }
        return;
    }
    library_log().remove(book);
    library_log().flush();
    collections::forget_book(book);
    collections::flush();
    if (!book_id.empty() && s->on_book_deleted)
    {
        s->on_book_deleted(book_id);
    }
    s->search_hits.erase(std::remove(s->search_hits.begin(), s->search_hits.end(), book), s->search_hits.end());
    s->marked.erase(book.string());
    refresh_keeping_cursor(s);
}

// A name that is free in the folder: the file's own, else with (2), (3)... before the extension.
std::filesystem::path free_name(const std::filesystem::path &folder, const std::filesystem::path &file)
{
    std::filesystem::path target = folder / file.filename();
    std::error_code ec;
    for (int copy = 2; std::filesystem::exists(target, ec) && copy < 1000; ++copy)
    {
        target = folder / (file.stem().string() + " (" + std::to_string(copy) + ")" + file.extension().string());
    }
    return target;
}

// The books to act on: the marked ones that are still there, else the one under the cursor.
std::vector<std::filesystem::path> acting_books(const FSState *s, const std::filesystem::path &current)
{
    std::vector<std::filesystem::path> books;
    std::error_code ec;
    for (const std::string &name : s->marked)
    {
        if (std::filesystem::exists(name, ec))
        {
            books.emplace_back(name);
        }
    }
    if (books.empty())
    {
        books.push_back(current);
    }
    return books;
}

// Puts the copied or cut books in the folder shown. A cut book keeps its place, status and collections.
void paste_clipboard(FSState *s)
{
    if (s->clipboard.empty() || s->search_active)
    {
        return;
    }
    const bool move = s->clipboard_move;
    int done = 0;
    int failed = 0;
    for (size_t i = 0; i < s->clipboard.size(); ++i)
    {
        const std::filesystem::path from = s->clipboard[i];
        progress_screen::show(s->styling, move ? "Moving" : "Copying", from.filename().string(), static_cast<int>(i * 100 / s->clipboard.size()));
        std::error_code ec;
        if (!std::filesystem::exists(from, ec))
        {
            ++failed;
            continue;
        }
        if (move && from.parent_path() == s->path)
        {
            ++done; // already here
            continue;
        }
        const std::filesystem::path to = free_name(s->path, from);
        if (move)
        {
            std::filesystem::rename(from, to, ec);
            if (ec)
            {
                // another card or drive: copy, then remove
                ec.clear();
                std::filesystem::copy_file(from, to, ec);
                if (!ec)
                {
                    std::filesystem::remove(from, ec);
                    ec.clear();
                }
            }
            if (!ec)
            {
                library_log().rename(from, to);
                collections::rename_book(from, to);
            }
        }
        else
        {
            std::filesystem::copy_file(from, to, ec);
        }
        if (ec)
        {
            ++failed;
        }
        else
        {
            ++done;
        }
    }
    library_log().flush();
    collections::flush();
    s->clipboard.clear();
    s->marked.clear();
    refresh_path_entries(s);
    focus_first_item(s);
    if (s->on_info)
    {
        std::string text = std::to_string(done) + (move ? " moved" : " copied");
        if (failed > 0)
        {
            text += ", " + std::to_string(failed) + " failed";
        }
        s->on_info(move ? "Move" : "Copy", text);
    }
}

// Makes a folder in the folder shown.
void make_folder(FSState *s, const std::string &typed)
{
    std::string name = typed;
    while (!name.empty() && name.back() == ' ')
    {
        name.pop_back();
    }
    if (name.empty())
    {
        return;
    }
    if (name.find_first_of("/\\:*?\"<>|") != std::string::npos || name[0] == '.')
    {
        if (s->on_info)
        {
            s->on_info("New folder", "A folder name cannot start with a dot or contain / \\ : * ? \" < > |");
        }
        return;
    }
    std::error_code ec;
    const std::filesystem::path folder = s->path / name;
    if (std::filesystem::exists(folder, ec))
    {
        if (s->on_info)
        {
            s->on_info("New folder", "That name is already used here.");
        }
        return;
    }
    if (!std::filesystem::create_directory(folder, ec) || ec)
    {
        if (s->on_info)
        {
            s->on_info("New folder", "The folder could not be made.");
        }
        return;
    }
    refresh_path_entries(s);
    if (!focus_entry(s, name))
    {
        focus_first_item(s);
    }
}

// Shows the books of a collection in place of the folder.
void show_collection(FSState *s, const std::string &name)
{
    s->collection = name;
    s->results_title = "Collection: " + name;
    s->results_up = "Leave collection";
    s->search_hits = collections::books(name);
    s->search_active = true;
    refresh_path_entries(s);
    focus_first_item(s);
}

// The list of collections, and a way to start a new one.
void open_collections(FSState *s)
{
    const auto shelves = collections::names();
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const auto &name : shelves)
    {
        names.push_back(name);
        details.push_back(std::to_string(collections::count(name)) + " books");
    }
    names.push_back("New collection...");
    details.push_back("");
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title("Collections");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, shelves](uint32_t index) {
        if (index < shelves.size())
        {
            show_collection(s, shelves[index]);
            return;
        }
        s->on_open_view(std::make_shared<TextInputView>(
            "New collection",
            "",
            [s](const std::string &name) {
                std::string trimmed = name;
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                if (!trimmed.empty())
                {
                    collections::create(trimmed);
                    collections::flush();
                    show_collection(s, trimmed);
                }
            },
            s->styling,
            40
        ));
    });
    s->on_open_view(menu);
}

// Picks a collection for the book, or starts a new one with it.
void add_to_collection(FSState *s, const std::filesystem::path &book)
{
    const auto shelves = collections::names();
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const auto &name : shelves)
    {
        names.push_back(name);
        details.push_back(collections::contains(name, book) ? "In it" : "");
    }
    names.push_back("New collection...");
    details.push_back("");
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title("Add to collection");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, shelves, book](uint32_t index) {
        if (index < shelves.size())
        {
            collections::add(shelves[index], book);
            collections::flush();
            return;
        }
        s->on_open_view(std::make_shared<TextInputView>(
            "New collection",
            "",
            [book](const std::string &name) {
                std::string trimmed = name;
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                if (!trimmed.empty())
                {
                    collections::add(trimmed, book);
                    collections::flush();
                }
            },
            s->styling,
            40
        ));
    });
    s->on_open_view(menu);
}

// The books opened lately, newest first, with how far along each is and when it was last open.
void open_history(FSState *s)
{
    const auto books = library_log().recent(40);
    if (books.empty())
    {
        if (s->on_info)
        {
            s->on_info("Recent books", "No book has been opened yet.");
        }
        return;
    }
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const auto &book : books)
    {
        std::string title = book.stem().string();
        if (s->info)
        {
            s->info->load(book);
            if (const BookInfo *info = s->info->get(book))
            {
                if (!info->title.empty())
                {
                    title = info->title;
                }
            }
        }
        names.push_back(title);
        const LibraryLog::Entry *log = library_log().get(book);
        std::string detail;
        if (log)
        {
            detail = log->finished() ? std::string("Done") : std::to_string(log->percent) + "%";
            char date[32] = "";
            const std::time_t when = static_cast<std::time_t>(log->last_opened);
            std::tm local {};
            if (localtime_r(&when, &local))
            {
                std::strftime(date, sizeof(date), "%d %b %Y", &local);
            }
            detail += std::string(", ") + date;
        }
        details.push_back(detail);
    }
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title("Recent books");
    menu->set_close_on_select();
    menu->set_on_selection([s, books](uint32_t index) {
        if (index < books.size() && s->on_file_selected)
        {
            s->on_file_selected(books[index]);
        }
    });
    s->on_open_view(menu);
}

// The reading status of a book as the lists show it.
std::string status_label(const LibraryLog::Entry *log)
{
    if (!log)
    {
        return "Not started";
    }
    switch (log->status)
    {
        case LibraryLog::Status::ToRead:
            return "To read";
        case LibraryLog::Status::OnHold:
            return "On hold";
        default:
            break;
    }
    if (log->finished())
    {
        return "Finished";
    }
    return log->status == LibraryLog::Status::Reading || log->percent > 0 ? "Reading" : "Not started";
}

// 0 To read, 1 Reading, 2 On hold, 3 Finished, -1 none of them.
int status_shelf(const LibraryLog::Entry &log)
{
    if (log.status == LibraryLog::Status::ToRead) return 0;
    if (log.status == LibraryLog::Status::OnHold) return 2;
    if (log.finished()) return 3;
    if (log.status == LibraryLog::Status::Reading || log.last_opened > 0) return 1;
    return -1;
}

// The place the library is searched from.
std::filesystem::path library_root(const FSState *s)
{
    return s->top_folder.empty() ? s->path : s->top_folder;
}

// Every supported book under the library root, in the order the disk lists them.
std::vector<std::filesystem::path> library_files(const FSState *s)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(library_root(s), std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        std::error_code ec_file;
        if (it->is_regular_file(ec_file) && file_type_is_supported(it->path().filename().string()))
        {
            files.push_back(it->path());
        }
    }
    return files;
}

// The title the lists show for a book on disk, loading its info when needed.
std::string book_title_for(FSState *s, const std::filesystem::path &book)
{
    if (s->info)
    {
        s->info->load(book);
        if (const BookInfo *info = s->info->get(book))
        {
            if (!info->title.empty())
            {
                return info->title;
            }
        }
    }
    return book.stem().string();
}

// Puts a list of books in place of the folder, as a search or a collection does.
void show_results(FSState *s, const std::string &title, const std::string &up, const std::vector<std::filesystem::path> &hits)
{
    s->results_title = title;
    s->results_up = up;
    s->collection.clear();
    s->search_hits = hits;
    s->search_active = true;
    refresh_path_entries(s);
    focus_first_item(s);
}

// Books grouped by the status you gave them, or by how far along they are.
void browse_by_status(FSState *s)
{
    static const char *labels[] = {"To read", "Reading", "On hold", "Finished"};
    std::vector<std::filesystem::path> shelves[4];
    for (const auto &item : library_log().all())
    {
        const int shelf = status_shelf(item.second);
        if (shelf >= 0)
        {
            shelves[shelf].push_back(item.first);
        }
    }
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (int i = 0; i < 4; ++i)
    {
        names.push_back(labels[i]);
        details.push_back(std::to_string(shelves[i].size()) + " books");
    }
    const auto favorites = library_log().favorites();
    names.push_back("Favorites");
    details.push_back(std::to_string(favorites.size()) + " books");
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title("Books by status");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, shelves0 = shelves[0], shelves1 = shelves[1], shelves2 = shelves[2], shelves3 = shelves[3], favorites](uint32_t index) {
        const std::vector<std::filesystem::path> *chosen[5] = {&shelves0, &shelves1, &shelves2, &shelves3, &favorites};
        if (index >= 5)
        {
            return;
        }
        std::vector<std::pair<std::string, std::filesystem::path>> sorted;
        for (const auto &book : *chosen[index])
        {
            sorted.emplace_back(lowercase(book_title_for(s, book)), book);
        }
        std::sort(sorted.begin(), sorted.end());
        std::vector<std::filesystem::path> hits;
        for (const auto &item : sorted)
        {
            hits.push_back(item.second);
        }
        static const char *titles[] = {"To read", "Reading", "On hold", "Finished", "Favorites"};
        show_results(s, titles[index], "Back to folders", hits);
    });
    s->on_open_view(menu);
}

// Sets the status of one book from a short list.
void choose_status(FSState *s, const std::filesystem::path &book)
{
    const std::vector<std::string> names = {"Automatic (by progress)", "To read", "Reading", "On hold", "Finished"};
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_title("Status: " + status_label(library_log().get(book)));
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, book](uint32_t index) {
        static const LibraryLog::Status values[] = {
            LibraryLog::Status::Auto,
            LibraryLog::Status::ToRead,
            LibraryLog::Status::Reading,
            LibraryLog::Status::OnHold,
            LibraryLog::Status::Finished
        };
        if (index < 5)
        {
            library_log().set_status(book, values[index]);
            library_log().flush();
            refresh_keeping_cursor(s);
        }
    });
    s->on_open_view(menu);
}

// A name reduced to letters and digits, to spot the same book saved twice.
std::string normalized_stem(const std::filesystem::path &book)
{
    std::string key;
    for (char c : book.stem().string())
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return key;
}

// Books that look like copies of each other: the same name apart from punctuation and case,
// or the same file type and exact size. Copies are listed together.
void find_duplicates(FSState *s)
{
    const auto files = library_files(s);
    std::map<std::string, std::vector<size_t>> by_name;
    std::map<std::string, std::vector<size_t>> by_size;
    for (size_t i = 0; i < files.size(); ++i)
    {
        progress_screen::show(s->styling, "Looking for copies", "", static_cast<int>(i * 100 / files.size()));
        const std::string name = normalized_stem(files[i]);
        if (!name.empty())
        {
            by_name[name + "." + lowercase(files[i].extension().string())].push_back(i);
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(files[i], ec);
        if (!ec && size > 20000)
        {
            by_size[lowercase(files[i].extension().string()) + std::to_string(size)].push_back(i);
        }
    }
    std::vector<std::filesystem::path> hits;
    std::vector<bool> used(files.size(), false);
    const auto add_groups = [&](const std::map<std::string, std::vector<size_t>> &groups) {
        for (const auto &group : groups)
        {
            if (group.second.size() < 2)
            {
                continue;
            }
            for (size_t index : group.second)
            {
                if (!used[index] && hits.size() < 500)
                {
                    used[index] = true;
                    hits.push_back(files[index]);
                }
            }
        }
    };
    add_groups(by_name);
    add_groups(by_size);
    if (hits.empty())
    {
        if (s->on_info)
        {
            s->on_info("Copies", "No book looks like a copy of another.");
        }
        return;
    }
    show_results(s, "Copies", "Back to folders", hits);
}

// Books that have never been opened, by title.
void find_unopened(FSState *s)
{
    const auto files = library_files(s);
    std::vector<std::pair<std::string, std::filesystem::path>> found;
    for (size_t i = 0; i < files.size() && found.size() < 500; ++i)
    {
        progress_screen::show(s->styling, "Looking for new books", "", static_cast<int>(i * 100 / files.size()));
        const LibraryLog::Entry *log = library_log().get(files[i]);
        if (!log || log->last_opened == 0)
        {
            found.emplace_back(lowercase(book_title_for(s, files[i])), files[i]);
        }
    }
    if (found.empty())
    {
        if (s->on_info)
        {
            s->on_info("Not opened", "Every book has been opened.");
        }
        return;
    }
    std::sort(found.begin(), found.end());
    std::vector<std::filesystem::path> hits;
    for (const auto &item : found)
    {
        hits.push_back(item.second);
    }
    show_results(s, "Not opened yet", "Back to folders", hits);
}

// The name a language code is shown with in the grouped lists.
std::string language_display(const std::string &code)
{
    std::string base = lowercase(code);
    const size_t dash = base.find_first_of("-_");
    if (dash != std::string::npos)
    {
        base.resize(dash);
    }
    static const std::map<std::string, std::string> names = {
        {"en", "English"}, {"fr", "French"}, {"pt", "Portuguese"}, {"es", "Spanish"}, {"de", "German"},
        {"it", "Italian"}, {"nl", "Dutch"}, {"ru", "Russian"}, {"ja", "Japanese"}, {"zh", "Chinese"},
        {"ko", "Korean"}, {"pl", "Polish"}, {"sv", "Swedish"}, {"la", "Latin"}, {"eng", "English"},
        {"fra", "French"}, {"fre", "French"}, {"por", "Portuguese"}, {"spa", "Spanish"}, {"deu", "German"}, {"ger", "German"}
    };
    const auto found = names.find(base);
    if (found != names.end())
    {
        return found->second;
    }
    return base.empty() ? std::string() : code;
}

enum class GroupBy { Series, Author, Language };

// The library as groups of books by series, author or language. Pick a group to list its books:
// a series in the order of its numbers, the others by title. Books with no value go in one last group.
void browse_by_group(FSState *s, GroupBy by)
{
    if (!s->info)
    {
        return;
    }
    const char *heading = by == GroupBy::Series ? "Series" : (by == GroupBy::Author ? "Authors" : "Languages");
    const auto files = library_files(s);
    struct Member
    {
        std::filesystem::path book;
        std::string title;
        double number;
    };
    std::map<std::string, std::vector<Member>> groups;
    std::map<std::string, std::string> shown_names;
    std::vector<Member> without;
    for (size_t i = 0; i < files.size(); ++i)
    {
        progress_screen::show(s->styling, std::string("Reading ") + heading, "", static_cast<int>(i * 100 / files.size()));
        s->info->load(files[i]);
        const BookInfo *info = s->info->get(files[i]);
        std::string value;
        if (info)
        {
            value = by == GroupBy::Series ? info->series : (by == GroupBy::Author ? info->author : language_display(info->language));
        }
        Member member = {files[i], info && !info->title.empty() ? info->title : files[i].stem().string(), info ? info->series_number() : 0.0};
        if (value.empty())
        {
            without.push_back(member);
            continue;
        }
        const std::string key = lowercase(value);
        shown_names.emplace(key, value);
        groups[key].push_back(member);
    }
    if (groups.empty())
    {
        if (s->on_info)
        {
            s->on_info(heading, std::string("None of the books name a ") + (by == GroupBy::Series ? "series." : (by == GroupBy::Author ? "author." : "language.")));
        }
        return;
    }

    std::vector<std::string> keys;
    for (const auto &group : groups)
    {
        keys.push_back(group.first);
    }
    if (!without.empty())
    {
        keys.push_back(std::string()); // the last row
    }
    std::vector<std::string> names;
    std::vector<std::string> details;
    std::vector<std::vector<Member>> lists;
    for (const std::string &key : keys)
    {
        const std::vector<Member> &members = key.empty() ? without : groups[key];
        names.push_back(key.empty() ? std::string("(none)") : shown_names[key]);
        details.push_back(std::to_string(members.size()) + (members.size() == 1 ? " book" : " books"));
        lists.push_back(members);
    }
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title(heading);
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, by, names, lists](uint32_t index) mutable {
        if (index >= lists.size())
        {
            return;
        }
        std::vector<Member> members = lists[index];
        std::sort(members.begin(), members.end(), [by](const Member &a, const Member &b) {
            if (by == GroupBy::Series && a.number != b.number)
            {
                return a.number < b.number;
            }
            return lowercase(a.title) < lowercase(b.title);
        });
        std::vector<std::filesystem::path> hits;
        for (const Member &member : members)
        {
            hits.push_back(member.book);
        }
        show_results(s, names[index], "Back to folders", hits);
    });
    s->on_open_view(menu);
}

const char *BACKUP_FILE_NAME = "PixelReader backup.zip";

// The address of the OPDS catalog is kept in a small file next to the other saved settings.
std::string read_opds_address(const FSState *s)
{
    if (s->store_dir.empty())
    {
        return std::string();
    }
    std::ifstream in(s->store_dir / "opds_url");
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
    {
        line.pop_back();
    }
    return line;
}

void write_opds_address(const FSState *s, const std::string &address)
{
    if (s->store_dir.empty())
    {
        return;
    }
    std::ofstream out(s->store_dir / "opds_url", std::ios::trunc);
    out << address << "\n";
}

// Saves a book from the catalog in the Downloads folder of the library.
void opds_download(FSState *s, const opds::Entry &entry)
{
    const std::filesystem::path folder = library_root(s) / "Downloads";
    std::filesystem::path target = folder / opds::file_name_for(entry.title, entry.author, entry.extension);
    std::error_code ec;
    for (int n = 2; std::filesystem::exists(target, ec) && n < 100; ++n)
    {
        target = folder / opds::file_name_for(entry.title + " (" + std::to_string(n) + ")", entry.author, entry.extension);
    }
    const std::string title = entry.title;
    std::string error;
    progress_screen::show(s->styling, "Downloading", title, -1);
    const bool ok = opds::download(entry.href, target, error, [s, title](size_t got, size_t total) {
        progress_screen::show(
            s->styling,
            "Downloading",
            title,
            total > 0 ? static_cast<int>(got * 100 / total) : -1
        );
    });
    if (s->on_info)
    {
        s->on_info("Online books", ok ? "Saved in the Downloads folder:\n" + target.filename().string() : error);
    }
    if (ok && !s->search_active)
    {
        refresh_keeping_cursor(s);
    }
}

// Shows one page of the catalog. Choosing a catalog opens it on top, so B goes back one level.
void opds_open(FSState *s, const std::string &url)
{
    std::string body;
    std::string error;
    progress_screen::show(s->styling, "Online books", "Connecting...", -1);
    if (!opds::fetch(url, body, error, [s](size_t got, size_t) {
            progress_screen::show(s->styling, "Online books", std::to_string(got / 1024) + " KB", -1);
        }))
    {
        if (s->on_info)
        {
            s->on_info("Online books", error);
        }
        return;
    }
    auto feed = std::make_shared<opds::Feed>();
    if (!opds::parse_feed(body, url, *feed, error))
    {
        if (s->on_info)
        {
            s->on_info("Online books", error);
        }
        return;
    }
    if (feed->entries.empty() && feed->next.empty())
    {
        if (s->on_info)
        {
            s->on_info("Online books", "This catalog has nothing to list.");
        }
        return;
    }
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const opds::Entry &entry : feed->entries)
    {
        names.push_back((entry.is_book ? "" : "> ") + entry.title);
        std::string detail = entry.author;
        if (entry.is_book)
        {
            std::string upper = entry.extension;
            for (char &c : upper)
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            detail += (detail.empty() ? "" : "  ") + upper;
        }
        details.push_back(detail);
    }
    if (!feed->next.empty())
    {
        names.push_back("More...");
        details.push_back("");
    }
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title(feed->title.empty() ? "Online books" : feed->title);
    menu->set_hints({"A  Open or get", "B  Back"});
    menu->set_on_selection([s, feed](uint32_t index) {
        if (index < feed->entries.size())
        {
            const opds::Entry &entry = feed->entries[index];
            if (entry.is_book)
            {
                opds_download(s, entry);
            }
            else
            {
                opds_open(s, entry.href);
            }
        }
        else if (!feed->next.empty())
        {
            opds_open(s, feed->next);
        }
    });
    s->on_open_view(menu);
}

void opds_ask_address(FSState *s)
{
    const std::string current = read_opds_address(s);
    s->on_open_view(std::make_shared<TextInputView>(
        "Catalog address (http://...)",
        current.empty() ? std::string("http://") : current,
        [s](const std::string &typed) {
            std::string address = typed;
            while (!address.empty() && address.back() == ' ') address.pop_back();
            if (address.empty() || address == "http://")
            {
                return;
            }
            write_opds_address(s, address);
            opds_open(s, address);
        },
        s->styling,
        200
    ));
}

void open_online_books(FSState *s)
{
    if (s->store_dir.empty())
    {
        if (s->on_info)
        {
            s->on_info("Online books", "There is no place to keep the address.");
        }
        return;
    }
    const std::string address = read_opds_address(s);
    if (address.empty())
    {
        opds_ask_address(s);
        return;
    }
    std::vector<std::string> names = {"Open the catalog", "Change the address"};
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_title("Online books");
    menu->set_details({address, ""});
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, address](uint32_t index) {
        if (index == 0)
        {
            opds_open(s, address);
        }
        else
        {
            opds_ask_address(s);
        }
    });
    s->on_open_view(menu);
}

// One highlight of one book, for the list of all of them.
struct HighlightRow
{
    std::filesystem::path book;
    std::string title;
    std::string text;
    DocAddr address = 0;
};

// The list of highlights, narrowed to those holding the words of the search when there are any.
void show_highlight_rows(FSState *s, const std::vector<HighlightRow> &rows, const std::string &search)
{
    const std::string wanted = lookup_lowercase(search);
    std::vector<HighlightRow> shown;
    for (const HighlightRow &row : rows)
    {
        if (wanted.empty() || lookup_lowercase(row.text).find(wanted) != std::string::npos || lookup_lowercase(row.title).find(wanted) != std::string::npos)
        {
            shown.push_back(row);
        }
    }
    std::vector<std::string> names = {search.empty() ? "Search..." : "Search again... (" + search + ")"};
    std::vector<std::string> details = {""};
    for (const HighlightRow &row : shown)
    {
        std::string one_line = row.text;
        for (char &c : one_line)
        {
            if (c == '\n' || c == '\t') c = ' ';
        }
        names.push_back(one_line);
        details.push_back(row.title);
    }
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title("All highlights (" + std::to_string(shown.size()) + ")");
    menu->set_compact(true);
    menu->set_hints({"A  Open the book", "B  Back"});
    menu->set_on_selection([s, rows, shown, search](uint32_t index) {
        if (index == 0)
        {
            s->on_open_view(std::make_shared<TextInputView>(
                "Search highlights",
                search,
                [s, rows](const std::string &typed) { show_highlight_rows(s, rows, typed); },
                s->styling,
                60
            ));
            return;
        }
        if (index - 1 < shown.size() && s->on_open_at)
        {
            s->on_open_at(shown[index - 1].book, shown[index - 1].address);
        }
    });
    s->on_open_view(menu);
}

// Every highlight of every book that has been opened, newest book first.
void open_all_highlights(FSState *s)
{
    std::vector<std::pair<std::filesystem::path, LibraryLog::Entry>> books = library_log().all();
    std::stable_sort(books.begin(), books.end(), [](const auto &a, const auto &b) {
        return a.second.last_opened > b.second.last_opened;
    });
    std::vector<HighlightRow> rows;
    for (const auto &book : books)
    {
        if (book.second.book_id.empty() || s->store_dir.empty())
        {
            continue;
        }
        const auto file = s->store_dir / "books" / (book.second.book_id + ".highlights");
        std::error_code ec;
        if (!std::filesystem::exists(file, ec))
        {
            continue;
        }
        const std::string title = book_title_for(s, book.first);
        for (const Highlight &highlight : load_highlights(file))
        {
            if (!highlight.text.empty())
            {
                rows.push_back({book.first, title, highlight.text, highlight.first});
            }
            if (rows.size() >= 1000)
            {
                break;
            }
        }
    }
    if (rows.empty())
    {
        if (s->on_info)
        {
            s->on_info("All highlights", "There are no highlights yet.\nHighlight words in a book and they are listed here.");
        }
        return;
    }
    show_highlight_rows(s, rows, "");
}

// The words around each match in every book of the library, so a phrase can be found when the
// book is not known. Choosing a match opens that book at the place.
void search_all_books(FSState *s, const std::string &query)
{
    constexpr size_t MAX_HITS = 300;
    constexpr size_t PER_BOOK = 6;
    const std::vector<std::filesystem::path> files = library_files(s);
    std::vector<HighlightRow> rows;
    for (size_t i = 0; i < files.size() && rows.size() < MAX_HITS; ++i)
    {
        const std::filesystem::path &book = files[i];
        const std::string title = book_title_for(s, book);
        const auto overall = [&](int percent) {
            return static_cast<int>((i * 100 + static_cast<size_t>(std::clamp(percent, 0, 100))) / files.size());
        };
        progress_screen::show(s->styling, "Searching all books", title, overall(0));
        std::shared_ptr<DocReader> reader;
        try
        {
            reader = create_doc_reader(book);
        }
        catch (...)
        {
            continue;
        }
        if (!reader)
        {
            continue;
        }
        std::vector<SearchHit> hits;
        try
        {
            hits = search_book(*reader, query, PER_BOOK, [&](int percent) {
                progress_screen::show(s->styling, "Searching all books", title, overall(percent));
            });
        }
        catch (...)
        {
            continue;
        }
        for (const SearchHit &hit : hits)
        {
            if (rows.size() >= MAX_HITS)
            {
                break;
            }
            HighlightRow row;
            row.book = book;
            row.title = title;
            row.text = hit.snippet;
            row.address = hit.address;
            rows.push_back(row);
        }
    }
    if (rows.empty())
    {
        if (s->on_info)
        {
            s->on_info("Search all books", "No match for \"" + query + "\" in " + std::to_string(files.size()) + " books.");
        }
        return;
    }
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const HighlightRow &row : rows)
    {
        std::string one_line = row.text;
        for (char &c : one_line)
        {
            if (c == '\n' || c == '\t') c = ' ';
        }
        names.push_back(one_line);
        details.push_back(row.title);
    }
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_details(details);
    menu->set_title(query + " (" + std::to_string(rows.size()) + (rows.size() >= MAX_HITS ? "+" : "") + ")");
    menu->set_compact(true);
    menu->set_hints({"A  Open the book", "B  Back"});
    menu->set_on_selection([s, rows](uint32_t index) {
        if (index < rows.size() && s->on_open_at)
        {
            s->on_open_at(rows[index].book, rows[index].address);
        }
    });
    s->on_open_view(menu);
}

void open_library_tools(FSState *s)
{
    const std::vector<std::string> names = {
        "Books by status", "Find copies", "Books not opened yet", "Back up settings and notes", "Restore from backup", "Get books online (OPDS)", "Receive books over wifi", "Browse by series", "Browse by author", "Browse by language", "Reading history", "All highlights", "Search inside all books"
    };
    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_title("Library tools");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s](uint32_t index) {
        const std::filesystem::path zip = library_root(s) / BACKUP_FILE_NAME;
        switch (index)
        {
            case 0:
                browse_by_status(s);
                break;
            case 1:
                find_duplicates(s);
                break;
            case 2:
                find_unopened(s);
                break;
            case 3:
            {
                if (s->store_dir.empty())
                {
                    return;
                }
                library_log().flush();
                collections::flush();
                if (s->flush_state)
                {
                    s->flush_state();
                }
                const backup::Result result = backup::write(s->store_dir, zip);
                if (s->on_info)
                {
                    s->on_info(
                        "Back up",
                        result.ok ? "Saved " + std::to_string(result.files) + " files to\n" + zip.string() : result.message
                    );
                }
                break;
            }
            case 4:
            {
                std::error_code ec;
                if (s->store_dir.empty() || !std::filesystem::exists(zip, ec))
                {
                    if (s->on_info)
                    {
                        s->on_info("Restore", "There is no backup. Put the file\n" + std::string(BACKUP_FILE_NAME) + "\nin the books folder first.");
                    }
                    return;
                }
                auto confirm = std::make_shared<SelectionMenu>(
                    std::vector<std::string> {"Keep what I have", "Restore, then close the reader"},
                    s->styling
                );
                confirm->set_title("Replace your settings and notes?");
                confirm->set_compact(true);
                confirm->set_close_on_select();
                confirm->set_on_selection([s, zip](uint32_t choice) {
                    if (choice != 1)
                    {
                        return;
                    }
                    const backup::Result result = backup::restore(s->store_dir, zip);
                    if (!result.ok)
                    {
                        if (s->on_info)
                        {
                            s->on_info("Restore", result.message);
                        }
                        return;
                    }
                    progress_screen::show(s->styling, "Restored", std::to_string(result.files) + " files. Closing...", 100);
                    SDL_Delay(2500);
                    std::_Exit(0); // leave without saving, so the restored files stay as they are
                });
                s->on_open_view(confirm);
                break;
            }
            case 5:
                open_online_books(s);
                break;
            case 6:
                s->on_open_view(std::make_shared<UploadView>(library_root(s) / "Downloads", s->styling, [s]() {
                    if (!s->search_active)
                    {
                        refresh_keeping_cursor(s);
                    }
                }));
                break;
            case 7:
                browse_by_group(s, GroupBy::Series);
                break;
            case 8:
                browse_by_group(s, GroupBy::Author);
                break;
            case 9:
                browse_by_group(s, GroupBy::Language);
                break;
            case 10:
                s->on_open_view(std::make_shared<ReadingHistoryView>(s->styling, [s](const std::string &path) {
                    return book_title_for(s, std::filesystem::path(path));
                }));
                break;
            case 11:
                open_all_highlights(s);
                break;
            case 12:
                s->on_open_view(std::make_shared<TextInputView>(
                    "Search inside all books",
                    "",
                    [s](const std::string &typed) {
                        std::string query = typed;
                        while (!query.empty() && query.back() == ' ') query.pop_back();
                        while (!query.empty() && query.front() == ' ') query.erase(0, 1);
                        if (!query.empty())
                        {
                            search_all_books(s, query);
                        }
                    },
                    s->styling,
                    60
                ));
                break;
            default:
                break;
        }
    });
    s->on_open_view(menu);
}
// The Y menu: properties, and for a book its favorite and finished marks, plus
// whether finished books are listed at all.
void open_options(FSState *s)
{
    if (!s->on_open_view || s->path_entries.empty() || s->cursor >= s->path_entries.size())
    {
        show_info(s);
        return;
    }
    const FSEntry entry = s->path_entries[s->cursor];
    if (entry.is_dir && entry.name == "..")
    {
        return;
    }
    const std::filesystem::path book = s->path / entry.name;
    const LibraryLog::Entry *log = library_log().get(book);

    enum class Act { Filter, Properties, Favorite, Status, LibraryTools, HideFinished, Search, ClearSearch, Rename, Delete, History, Collections, AddToCollection, RemoveFromCollection, DeleteCollection, Mark, CopyBooks, MoveBooks, DeleteBooks, ClearMarks, Paste, NewFolder, ResetView };
    std::vector<Act> acts = {Act::Properties};
    std::vector<std::string> names = {"Properties"};
    if (!entry.is_dir)
    {
        acts.push_back(Act::Favorite);
        names.push_back(log && log->favorite ? "Remove from favorites" : "Add to favorites");
        acts.push_back(Act::Status);
        names.push_back("Status: " + status_label(log));
    }
    acts.push_back(Act::HideFinished);
    names.push_back(s->hide_finished ? "Show finished books" : "Hide finished books");
    if (!s->search_active)
    {
        acts.push_back(Act::Filter);
        names.push_back(s->filter == 0 ? std::string("Show: all books") : std::string("Show: ") + filter_label(s->filter) + " only");
    }
    if (!s->search_active && folder_prefs::get(s->prefs_for))
    {
        acts.push_back(Act::ResetView);
        names.push_back("Use the default view in this folder");
    }
    acts.push_back(Act::History);
    names.push_back("Recent books");
    acts.push_back(Act::Search);
    names.push_back("Search library");
    acts.push_back(Act::Collections);
    names.push_back("Collections");
    acts.push_back(Act::LibraryTools);
    names.push_back("Library tools");
    if (s->search_active)
    {
        acts.push_back(Act::ClearSearch);
        names.push_back(s->collection.empty() ? "Clear search" : "Leave collection");
    }
    if (!entry.is_dir)
    {
        acts.push_back(Act::AddToCollection);
        names.push_back("Add to collection");
        if (!s->collection.empty())
        {
            acts.push_back(Act::RemoveFromCollection);
            names.push_back("Remove from collection");
        }
    }
    if (!s->collection.empty())
    {
        acts.push_back(Act::DeleteCollection);
        names.push_back("Delete collection");
    }
    if (!entry.is_dir)
    {
        acts.push_back(Act::Rename);
        names.push_back("Rename file");
        acts.push_back(Act::Delete);
        names.push_back("Delete book");
        const bool is_marked = s->marked.count(book.string()) > 0;
        acts.push_back(Act::Mark);
        names.push_back(is_marked ? "Unmark" : "Mark for copy, move or delete");
    }
    {
        // The books to copy, move or delete: the marked ones, else the one under the cursor
        const size_t marks = s->marked.size();
        if (marks > 0 || !entry.is_dir)
        {
            const std::string what = marks > 0 ? " marked (" + std::to_string(marks) + ")" : " this book";
            acts.push_back(Act::CopyBooks);
            names.push_back("Copy" + what);
            acts.push_back(Act::MoveBooks);
            names.push_back("Move" + what);
            if (marks > 0)
            {
                acts.push_back(Act::DeleteBooks);
                names.push_back("Delete" + what);
                acts.push_back(Act::ClearMarks);
                names.push_back("Clear marks");
            }
        }
        if (!s->search_active)
        {
            if (!s->clipboard.empty())
            {
                acts.push_back(Act::Paste);
                names.push_back("Paste here (" + std::to_string(s->clipboard.size()) + (s->clipboard_move ? " to move)" : " to copy)"));
            }
            acts.push_back(Act::NewFolder);
            names.push_back("New folder");
        }
    }

    auto menu = std::make_shared<SelectionMenu>(names, s->styling);
    menu->set_title(entry.is_dir ? folder_title(book) : entry_title(s, entry));
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([s, acts, book](uint32_t index) {
        if (index >= acts.size())
        {
            return;
        }
        const LibraryLog::Entry *now = library_log().get(book);
        switch (acts[index])
        {
            case Act::Properties:
                show_info(s);
                break;
            case Act::Favorite:
                library_log().set_favorite(book, !(now && now->favorite));
                library_log().flush();
                refresh_keeping_cursor(s);
                break;
            case Act::Status:
                choose_status(s, book);
                break;
            case Act::LibraryTools:
                open_library_tools(s);
                break;
            case Act::History:
                open_history(s);
                break;
            case Act::Search:
                s->on_open_view(std::make_shared<TextInputView>(
                    "Search library",
                    s->search_query,
                    [s](const std::string &query) { run_search(s, query); },
                    s->styling
                ));
                break;
            case Act::Collections:
                open_collections(s);
                break;
            case Act::AddToCollection:
                add_to_collection(s, book);
                break;
            case Act::RemoveFromCollection:
                collections::remove(s->collection, book);
                collections::flush();
                s->search_hits.erase(std::remove(s->search_hits.begin(), s->search_hits.end(), book), s->search_hits.end());
                refresh_keeping_cursor(s);
                break;
            case Act::DeleteCollection:
                collections::delete_collection(s->collection);
                collections::flush();
                s->search_active = false;
                s->search_hits.clear();
                s->collection.clear();
                refresh_path_entries(s);
                focus_first_item(s);
                break;
            case Act::ClearSearch:
                s->search_active = false;
                s->collection.clear();
                s->search_hits.clear();
                refresh_path_entries(s);
                focus_first_item(s);
                break;
            case Act::Rename:
                s->on_open_view(std::make_shared<TextInputView>(
                    "Rename file",
                    book.stem().string(),
                    [s, book](const std::string &name) { rename_book(s, book, name); },
                    s->styling,
                    120
                ));
                break;
            case Act::Delete:
            {
                auto confirm = std::make_shared<SelectionMenu>(std::vector<std::string> {"Keep the book", "Delete it for good"}, s->styling);
                confirm->set_title("Delete " + book.filename().string() + "?");
                confirm->set_compact(true);
                confirm->set_close_on_select();
                confirm->set_on_selection([s, book](uint32_t choice) {
                    if (choice == 1)
                    {
                        delete_book(s, book);
                    }
                });
                s->on_open_view(confirm);
                break;
            }
            case Act::Mark:
                if (!s->marked.erase(book.string()))
                {
                    s->marked.insert(book.string());
                }
                refresh_keeping_cursor(s);
                break;
            case Act::ClearMarks:
                s->marked.clear();
                refresh_keeping_cursor(s);
                break;
            case Act::CopyBooks:
            case Act::MoveBooks:
                s->clipboard = acting_books(s, book);
                s->clipboard_move = acts[index] == Act::MoveBooks;
                if (s->on_info)
                {
                    s->on_info(
                        s->clipboard_move ? "Move" : "Copy",
                        std::to_string(s->clipboard.size()) + (s->clipboard.size() == 1 ? " book is" : " books are")
                            + " ready.\nOpen the folder to put them in, then choose Paste here from the options."
                    );
                }
                break;
            case Act::Paste:
                paste_clipboard(s);
                break;
            case Act::NewFolder:
                s->on_open_view(std::make_shared<TextInputView>(
                    "New folder",
                    "",
                    [s](const std::string &name) { make_folder(s, name); },
                    s->styling,
                    60
                ));
                break;
            case Act::DeleteBooks:
            {
                const std::vector<std::filesystem::path> doomed = acting_books(s, book);
                auto confirm = std::make_shared<SelectionMenu>(std::vector<std::string> {"Keep them", "Delete them for good"}, s->styling);
                confirm->set_title("Delete " + std::to_string(doomed.size()) + (doomed.size() == 1 ? " book?" : " books?"));
                confirm->set_compact(true);
                confirm->set_close_on_select();
                confirm->set_on_selection([s, doomed](uint32_t choice) {
                    if (choice == 1)
                    {
                        for (const auto &one : doomed)
                        {
                            delete_book(s, one);
                        }
                        s->marked.clear();
                        refresh_keeping_cursor(s);
                    }
                });
                s->on_open_view(confirm);
                break;
            }
            case Act::ResetView:
                folder_prefs::forget(s->prefs_for);
                s->prefs_for.clear();
                refresh_keeping_cursor(s);
                break;
            case Act::Filter:
                // All books, then not opened, in progress, favorites and finished, and round again.
                s->filter = (s->filter + 1) % 5;
                refresh_keeping_cursor(s);
                break;
            case Act::HideFinished:
                s->hide_finished = !s->hide_finished;
                note_view_choice(s, -1, -1, s->hide_finished ? 1 : 0);
                refresh_keeping_cursor(s);
                break;
        }
    });
    s->on_open_view(menu);
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
    if (!s->on_open_view)
    {
        s->on_info("Properties", properties_text(s, entry));
        return;
    }

    // The page of facts about the book, with its cover when one is ready
    const std::filesystem::path book = s->path / entry.name;
    std::vector<std::pair<std::string, std::string>> facts;
    std::string description;
    std::string heading = entry.name;
    const BookInfo *info = (!entry.is_dir && s->info) ? s->info->get(book) : nullptr;
    if (info)
    {
        facts.push_back({"Title", info->title});
        facts.push_back({"Author", info->author});
        if (!info->series.empty())
        {
            facts.push_back({"Series", info->series + (info->series_index.empty() ? std::string() : " #" + info->series_index)});
        }
        facts.push_back({"Language", info->language});
        facts.push_back({"Publisher", info->publisher});
        facts.push_back({"Published", info->date});
        description = info->description;
        if (!info->title.empty())
        {
            heading = info->title;
        }
    }
    if (entry.is_dir)
    {
        facts.push_back({"Type", "Folder"});
    }
    else
    {
        const std::string kind = book_kind(entry.name);
        facts.push_back({"Type", kind.empty() ? std::string("Book") : kind + " book"});
        facts.push_back({"Size", format_size(entry.size)});
        const LibraryLog::Entry *log = library_log().get(book);
        facts.push_back({"Status", status_label(log)});
        if (log && (log->percent > 0 || log->finished()))
        {
            facts.push_back({"Progress", std::to_string(log->finished() ? 100 : log->percent) + "%"});
        }
        facts.push_back({"Reading time", reading_time_text(entry, log)});
        struct stat st;
        if (::stat(book.c_str(), &st) == 0)
        {
            char day[16];
            const time_t modified = st.st_mtime;
            if (std::strftime(day, sizeof(day), "%Y-%m-%d", std::localtime(&modified)) > 0)
            {
                facts.push_back({"File date", day});
            }
        }
    }
    facts.push_back({"File", entry.name});
    facts.push_back({"Folder", s->path.string()});
    SDL_Surface *cover = nullptr;
    if (s->covers && !entry.is_dir)
    {
        cover = s->covers->get(book);
        if (!cover)
        {
            s->covers->want({book});
        }
    }
    s->on_open_view(std::make_shared<BookInfoView>(heading, std::move(facts), description, cover, s->styling));
}

// Go to the parent folder, with the cursor on the folder we came from.
void go_up(FSState *s)
{
    if (s->search_active)
    {
        s->search_active = false;
        s->search_hits.clear();
        s->collection.clear();
        refresh_path_entries(s);
        focus_first_item(s);
        return;
    }
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

FileSelector::FileSelector(std::filesystem::path path, SystemStyling &styling, std::filesystem::path top_folder)
    : state(std::make_unique<FSState>(
          sanitize_starting_path(path),
          styling
      ))
{
    if (!top_folder.empty())
    {
        top_folder = top_folder.lexically_normal();
        if (!top_folder.has_filename())
        {
            top_folder = top_folder.parent_path(); // drop the trailing separator
        }
        state->top_folder = top_folder;
    }

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
    const bool changed = load_book_info(state.get());
    if (state->grid_mode)
    {
        return state->grid.tick(elapsed_ms) || changed;
    }
    return state->menu.tick(elapsed_ms) || changed;
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
        open_options(state.get());
        return;
    }
    if (key == SW_BTN_SELECT)
    {
        set_grid_view(!state->grid_mode);
        note_view_choice(state.get(), -1, state->grid_mode ? 1 : 0, -1);
        return;
    }
    if (key == SW_BTN_START)
    {
        set_sort(next_browser_sort(state->sort));
        note_view_choice(state.get(), static_cast<int>(state->sort), -1, -1);
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
    state->global_grid = grid;
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
}

void FileSelector::set_sort(BrowserSort sort)
{
    state->global_sort = sort;
    state->sort = sort;
    resort_keeping_cursor(state.get());
}

void FileSelector::set_on_sort_change(std::function<void(BrowserSort)> callback)
{
    state->on_sort_change = callback;
}

bool FileSelector::grid_view() const
{
    return state->grid_mode;
}

void FileSelector::set_on_view_mode_change(std::function<void(bool)> callback)
{
    state->on_view_mode_change = callback;
}

void FileSelector::set_on_open_at(std::function<void(const std::filesystem::path &, DocAddr)> callback)
{
    state->on_open_at = callback;
}

void FileSelector::set_on_open_view(std::function<void(std::shared_ptr<View>)> callback)
{
    state->on_open_view = callback;
}

void FileSelector::set_hide_finished(bool hide)
{
    state->global_hide = hide;
    state->hide_finished = hide;
    refresh_path_entries(state.get());
    focus_first_item(state.get());
}

void FileSelector::set_on_hide_finished_change(std::function<void(bool)> callback)
{
    state->on_hide_finished_change = callback;
}

void FileSelector::set_on_book_deleted(std::function<void(const std::string &)> callback)
{
    state->on_book_deleted = callback;
}

void FileSelector::set_on_info(std::function<void(const std::string &, const std::string &)> callback)
{
    state->on_info = callback;
}

void FileSelector::set_flush_state(std::function<void()> callback)
{
    state->flush_state = callback;
}

void FileSelector::set_cover_cache_dir(std::filesystem::path dir)
{
    state->store_dir = dir.parent_path();
    state->covers = std::make_unique<BookCovers>(dir);
    state->grid.set_covers(state->covers.get());
}

void FileSelector::set_info_cache_dir(std::filesystem::path dir)
{
    state->info = std::make_unique<BookInfoCache>(std::move(dir));
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
