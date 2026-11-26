#include "./reader_view.h"

#include "./definition_view.h"
#include "./goto_view.h"
#include "./book_map_view.h"
#include "./gallery_view.h"
#include "./grouped_menu.h"
#include "./image_view.h"
#include "./page_browser_view.h"
#include "./text_input_view.h"
#include "./popup_view.h"
#include "./selection_menu.h"
#include "./menu_draw.h"
#include "reader/progress_screen.h"
#include "reader/accent_fold.h"
#include "reader/button_actions.h"
#include "reader/color_theme_def.h"
#include "reader/layout_profiles.h"
#include "reader/book_info.h"
#include "reader/book_layout.h"
#include "reader/bookmarks.h"
#include "reader/book_layout.h"
#include "reader/views/guide_view.h"
#include "./token_view/token_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/dictionary.h"
#include "reader/next_book.h"
#include "reader/screen_dim.h"
#include "filetypes/epub/book_style.h"
#include "reader/book_info.h"
#include "reader/text_style.h"
#include "reader/library_log.h"
#include "reader/book_search.h"
#include "reader/reading_stats.h"
#include "reader/vocabulary.h"
#include "reader/word_forms.h"
#include "reader/reading_mode.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "reader/work_pump.h"
#include "reader/kosync.h"
#include "reader/view_stack.h"
#include "reader/wifi_upload.h"
#include "./reading_history_view.h"

#include "doc_api/doc_reader.h"
#include <cmath>
#include <ctime>
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <SDL/SDL.h>

#include <algorithm>
#include <deque>
#include <limits>
#include <functional>
#include <set>
#include <fstream>
#include <map>
#include <optional>
#include <iostream>

namespace
{

// Longest the page count may hold the main loop in one frame.
constexpr Uint32 PAGINATION_SLICE_MS = 50;

// Longest a key may wait for the count of the stretch it just entered.
constexpr Uint32 PAGINATION_SETTLE_MS = 150;

} // namespace

// What a deletion took away, so Undo can bring it back.
struct UndoEntry
{
    std::string what;
    std::vector<Highlight> highlights; // deleted, or with the note they had before it was changed
    std::vector<Bookmark> bookmarks;
    struct Word
    {
        vocabulary::Entry entry;
        std::string sentence;
    };
    std::vector<Word> words;
};

struct ReaderViewState
{
    bool is_done = false;
    std::vector<UndoEntry> undo; // the last deletions, newest last; gone when the book is closed
    bool highlights_by_date = false; // the highlight list shows the newest first

    std::function<void(DocAddr)> on_change_address;

    std::string filename;
    std::shared_ptr<DocReader> reader;
    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t token_view_styling_sub_id;

    ViewStack &view_stack;

    std::filesystem::path book_path;
    std::string book_title;
    // A button that has a hold action waits to see whether it is a tap or a hold: the tap runs when it is released.
    SDLKey pending_key = SDLK_UNKNOWN;
    bool pending_fired = false;
    bool replaying_tap = false;
    std::string lookup_sentence; // the sentence around the word being looked up, kept with it in the word list
    std::string dictionary_name = "english"; // follows the language of the book
    std::string own_dictionary_name = "english"; // what the book itself says, before a choice for this book
    std::filesystem::path highlights_path;
    std::filesystem::path bookmarks_path;
    std::vector<Bookmark> bookmarks;
    std::vector<DocAddr> jump_stack;
    // The matches of the last search, so they can be stepped through without the list.
    std::shared_ptr<std::vector<SearchHit>> search_hits;
    size_t search_index = 0;
    std::string search_query;
    // After Next mark or Previous mark: the mark that was reached and where the view stood on landing.
    std::optional<DocAddr> landed_mark;
    DocAddr landed_view = 0;
    std::unique_ptr<TokenView> token_view;
    // The count of pages is on screen instead of the text, and the percent drawn.
    bool counting_shown = false;
    int counting_drawn = -1;
    bool sync_checked = false; // the server was asked about this book once, when it opened

    ReaderViewState(std::filesystem::path path, DocAddr seek_address, std::shared_ptr<DocReader> reader, SystemStyling &sys_styling, TokenViewStyling &token_view_styling, uint32_t token_view_styling_sub_id, ViewStack &view_stack, std::filesystem::path highlights_path)
        : filename(path.filename()),
          book_path(path),
          reader(reader),
          sys_styling(sys_styling),
          token_view_styling(token_view_styling),
          token_view_styling_sub_id(token_view_styling_sub_id),
          view_stack(view_stack),
          highlights_path(std::move(highlights_path)),
          token_view(std::make_unique<TokenView>(
              reader,
              seek_address,
              sys_styling,
              token_view_styling,
              this->highlights_path
          ))
    {
    }

    ~ReaderViewState()
    {
    }
};

namespace
{

DocAddr get_current_address(const ReaderViewState &state)
{
    if (state.token_view)
    {
        return state.token_view->get_address();
    }

    return 0;
}

// The chapter list, with how long each chapter is and how far along you are. X folds the sub-chapters
// away, so a book with many parts shows only the parts.
void show_message(ReaderViewState &state, const std::string &message);

// The language of lookups and hyphenation: the choice made for this book, else what the book says.
void apply_book_language(ReaderViewState &state)
{
    const std::string &chosen = state.token_view_styling.get_book_language();
    const std::string wanted = chosen.empty() ? state.own_dictionary_name : chosen;
    if (wanted != state.dictionary_name)
    {
        state.dictionary_name = wanted;
        if (state.token_view)
        {
            state.token_view->set_hyphenation_language(wanted);
        }
    }
}

// The chapter list. Parts with sections under them can be folded: collapsed holds the folded ones.
// keep_row puts the cursor on that row, for when the list is drawn again after a change.
void open_toc_menu(
    ReaderView &reader_view,
    ReaderViewState &state,
    std::set<uint32_t> collapsed = {},
    const std::string &filter = std::string(),
    int keep_row = -1
)
{
    const auto &toc = state.reader->get_table_of_contents();
    if (toc.empty())
    {
        return;
    }

    const uint32_t current_toc_index = state.reader->get_toc_position(get_current_address(state)).toc_index;
    bool has_nested = false;
    for (const auto &toc_item : toc)
    {
        has_nested = has_nested || toc_item.indent_level > 0;
    }
    // A filter lists only the chapters with those words in their name, all at one level.
    const std::string wanted = lookup_lowercase(filter);
    if (!has_nested || !wanted.empty())
    {
        collapsed.clear();
    }
    // A chapter is a parent when the next one is nested deeper. A folded parent hides what is nested under it.
    std::vector<bool> is_parent(toc.size(), false);
    std::vector<bool> hidden(toc.size(), false);
    {
        bool hiding = false;
        uint32_t hide_level = 0;
        for (uint32_t i = 0; i < toc.size(); ++i)
        {
            is_parent[i] = i + 1 < toc.size() && toc[i + 1].indent_level > toc[i].indent_level;
            if (hiding && toc[i].indent_level > hide_level)
            {
                hidden[i] = true;
                continue;
            }
            hiding = false;
            if (is_parent[i] && collapsed.count(i))
            {
                hiding = true;
                hide_level = toc[i].indent_level;
            }
        }
    }

    // The rows shown, as indexes into the table of contents.
    auto visible = std::make_shared<std::vector<uint32_t>>();
    std::vector<std::string> menu_names;
    std::vector<std::string> menu_details;
    size_t marker_row = 0;
    for (uint32_t i = 0; i < toc.size(); ++i)
    {
        if (hidden[i])
        {
            continue;
        }
        if (!wanted.empty() && lookup_lowercase(toc[i].display_name).find(wanted) == std::string::npos)
        {
            continue;
        }
        if (i <= current_toc_index)
        {
            marker_row = visible->size();
        }
        visible->push_back(i);
        std::string mark;
        if (has_nested && wanted.empty())
        {
            mark = is_parent[i] ? (collapsed.count(i) ? "+ " : "- ") : "  ";
        }
        menu_names.push_back(std::string(wanted.empty() ? toc[i].indent_level * 2 : 0, ' ') + mark + toc[i].display_name);

        std::string detail;
        if (i < current_toc_index)
        {
            detail = "Read";
        }
        else if (i == current_toc_index)
        {
            detail = std::to_string(state.reader->get_toc_position(get_current_address(state)).progress_percent) + "%";
        }
        menu_details.push_back(detail);
    }

    if (visible->empty())
    {
        show_message(state, "No chapter has \"" + filter + "\" in its name");
        return;
    }
    auto toc_select_menu = std::make_shared<SelectionMenu>(menu_names, state.sys_styling);
    toc_select_menu->set_details(menu_details);
    toc_select_menu->set_on_selection([&reader_view, visible, last_toc_index = current_toc_index](uint32_t row) {
        if (row < visible->size() && (*visible)[row] != last_toc_index)
        {
            reader_view.seek_to_toc_index((*visible)[row]);
        }
    });
    toc_select_menu->set_close_on_select();
    toc_select_menu->set_title(!wanted.empty() ? "Chapters: " + filter : !collapsed.empty() ? "Chapters (folded)" : "Chapters");
    if (has_nested && wanted.empty())
    {
        toc_select_menu->set_uses_x(true);
        toc_select_menu->set_hints({"A  Go", "Start  Fold part", collapsed.empty() ? "X  Fold all" : "X  Unfold all", "Y  Find", "B  Close"});
    }
    else
    {
        toc_select_menu->set_hints({"A  Go", "Y  Find", "B  Close"});
    }

    // select current toc item
    toc_select_menu->set_marker(static_cast<uint32_t>(marker_row));
    toc_select_menu->set_cursor_pos(static_cast<uint32_t>(keep_row >= 0 && keep_row < static_cast<int>(visible->size()) ? keep_row : static_cast<int>(marker_row)));

    toc_select_menu->set_default_on_keypress([&reader_view, &state, collapsed, is_parent, visible, has_nested, filter](SDLKey key, SelectionMenu &toc) {
        if (key == SW_BTN_SELECT)
        {
            toc.close();
        }
        else if (key == SW_BTN_X && has_nested && filter.empty())
        {
            toc.close();
            std::set<uint32_t> next;
            if (collapsed.empty())
            {
                // Fold every part that has sections, keeping the one level of parts on show.
                const auto &items = state.reader->get_table_of_contents();
                for (uint32_t i = 0; i < items.size(); ++i)
                {
                    if (is_parent[i] && items[i].indent_level == 0)
                    {
                        next.insert(i);
                    }
                }
            }
            open_toc_menu(reader_view, state, next);
        }
        else if (key == SW_BTN_START && has_nested && filter.empty())
        {
            const uint32_t row = toc.get_cursor_pos();
            if (row >= visible->size() || !is_parent[(*visible)[row]])
            {
                return;
            }
            std::set<uint32_t> next = collapsed;
            const uint32_t index = (*visible)[row];
            if (!next.erase(index))
            {
                next.insert(index);
            }
            toc.close();
            open_toc_menu(reader_view, state, next, std::string(), static_cast<int>(row));
        }
        else if (key == SW_BTN_Y)
        {
            toc.close();
            state.view_stack.push(std::make_shared<TextInputView>(
                "Find a chapter",
                filter,
                [&reader_view, &state](const std::string &typed) {
                    std::string text = typed;
                    while (!text.empty() && text.back() == ' ') text.pop_back();
                    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
                    open_toc_menu(reader_view, state, {}, text);
                },
                state.sys_styling,
                40
            ));
        }
    });

    state.view_stack.push(toc_select_menu);
}
void show_message(ReaderViewState &state, const std::string &message)
{
    state.view_stack.push(std::make_shared<PopupView>(
        message,
        state.sys_styling.get_font_name(),
        state.sys_styling
    ));
}

std::string percent_label(const ReaderViewState &state, DocAddr address)
{
    return std::to_string(state.reader->get_global_progress_percent(address)) + "%";
}

// ---- Progress sync (a KOReader sync server)

// The place in the book as a fraction of the whole, for the server.
static double current_fraction(ReaderViewState &state)
{
    const auto extent = state.reader->get_progress_extent(get_current_address(state), false);
    return extent.second > 0 ? std::min(1.0, static_cast<double>(extent.first) / static_cast<double>(extent.second)) : 0.0;
}

// The settings, and whether a request can be made now. Says why not unless quiet.
static bool sync_ready(ReaderViewState &state, kosync::Config &config, bool quiet)
{
    config = kosync::load();
    if (!config.configured())
    {
        if (!quiet)
        {
            show_message(state, "Set up the sync server first");
        }
        return false;
    }
    if (local_ip_address().empty())
    {
        if (!quiet)
        {
            show_message(state, "No wifi connection");
        }
        return false;
    }
    return true;
}

static void sync_push(ReaderViewState &state, bool quiet)
{
    kosync::Config config;
    if (!sync_ready(state, config, quiet))
    {
        return;
    }
    progress_screen::show(state.sys_styling, "Sync", "Sending your place...", -1);
    std::string error;
    const bool ok = kosync::push(
        config,
        kosync::document_id(state.book_path),
        std::to_string(get_current_address(state)),
        current_fraction(state),
        error
    );
    state.token_view->invalidate();
    if (!quiet)
    {
        show_message(state, ok ? "Place sent" : error);
    }
}

// Asks the server where the book was left. A quiet call only speaks up when the server is further on.
static void sync_pull(ReaderView &reader_view, ReaderViewState &state, bool quiet)
{
    kosync::Config config;
    if (!sync_ready(state, config, quiet))
    {
        return;
    }
    progress_screen::show(state.sys_styling, "Sync", "Asking the server...", -1);
    kosync::Place place;
    std::string error;
    const bool ok = kosync::pull(config, kosync::document_id(state.book_path), place, error);
    state.token_view->invalidate();
    if (!ok)
    {
        if (!quiet)
        {
            show_message(state, error);
        }
        return;
    }
    if (!place.found)
    {
        if (!quiet)
        {
            show_message(state, "Nothing is saved on the server for this book yet");
        }
        return;
    }
    const int remote = static_cast<int>(place.percentage * 100.0 + 0.5);
    const int local = static_cast<int>(state.reader->get_global_progress_percent(get_current_address(state)));
    if (remote == local || (quiet && remote < local))
    {
        if (!quiet)
        {
            show_message(state, "You are already at the same place");
        }
        return;
    }
    // The address the other device wrote, when it agrees with the percent; else the percent
    DocAddr target = state.reader->get_address_at_percent(static_cast<uint32_t>(remote));
    if (!place.progress.empty() && place.progress.find_first_not_of("0123456789") == std::string::npos && place.progress.size() < 19)
    {
        const DocAddr written = static_cast<DocAddr>(std::strtoull(place.progress.c_str(), nullptr, 10));
        const int written_percent = static_cast<int>(state.reader->get_global_progress_percent(written));
        if (std::abs(written_percent - remote) <= 2)
        {
            target = written;
        }
    }
    auto menu = std::make_shared<SelectionMenu>(
        std::vector<std::string> {
            "Go to " + std::to_string(remote) + "%" + (place.device.empty() ? std::string() : " (" + place.device + ")"),
            "Stay at " + std::to_string(local) + "%"
        },
        state.sys_styling
    );
    menu->set_title("The server has another place");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, target](uint32_t index) {
        if (index == 0)
        {
            reader_view.jump_to(target);
        }
    });
    state.view_stack.push(menu);
}

// Server address, user name and password, one after the other, then a test.
static void sync_setup(ReaderViewState &state, int step, kosync::Config config)
{
    static const char *headings[3] = {"Server address (http://...)", "User name", "Password"};
    const std::string initial = step == 0 ? (config.server.empty() ? std::string("http://") : config.server)
        : step == 1 ? config.user : std::string();
    state.view_stack.push(std::make_shared<TextInputView>(
        headings[step],
        initial,
        [&state, step, config](const std::string &typed) mutable {
            std::string value = typed;
            while (!value.empty() && value.back() == ' ') value.pop_back();
            if (step == 0)
            {
                config.server = value;
            }
            else if (step == 1)
            {
                config.user = value;
            }
            else
            {
                config.key = kosync::md5_hex(value);
            }
            if (step < 2)
            {
                sync_setup(state, step + 1, config);
                return;
            }
            kosync::save(config);
            if (!config.configured())
            {
                show_message(state, "Saved, but something is missing");
                return;
            }
            if (local_ip_address().empty())
            {
                show_message(state, "Saved. There is no wifi to test it now.");
                return;
            }
            progress_screen::show(state.sys_styling, "Sync", "Testing the login...", -1);
            std::string error;
            const bool ok = kosync::check_login(config, error);
            state.token_view->invalidate();
            show_message(state, ok ? "Saved. The server accepted the login." : "Saved. " + error + ". If the user does not exist yet, use Create account.");
        },
        state.sys_styling,
        step == 0 ? 200 : 60
    ));
}

static void open_sync_menu(ReaderView &reader_view, ReaderViewState &state)
{
    const kosync::Config config = kosync::load();
    std::vector<std::string> names = {
        "Send my place now",
        "Get the place from the server",
        std::string("Sync when opening and leaving: ") + (config.auto_sync ? "On" : "Off"),
        "Server settings...",
        "Create account on the server"
    };
    auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
    menu->set_title(config.configured() ? "Sync: " + config.user : std::string("Sync (not set up)"));
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, config](uint32_t index) {
        switch (index)
        {
            case 0:
                sync_push(state, false);
                break;
            case 1:
                sync_pull(reader_view, state, false);
                break;
            case 2:
            {
                kosync::Config changed = config;
                changed.auto_sync = !changed.auto_sync;
                kosync::save(changed);
                open_sync_menu(reader_view, state);
                break;
            }
            case 3:
                sync_setup(state, 0, config);
                break;
            case 4:
            {
                kosync::Config now;
                if (!sync_ready(state, now, false))
                {
                    break;
                }
                progress_screen::show(state.sys_styling, "Sync", "Creating the account...", -1);
                std::string error;
                const bool ok = kosync::create_account(now, error);
                state.token_view->invalidate();
                show_message(state, ok ? "Account created" : error);
                break;
            }
            default:
                break;
        }
    });
    state.view_stack.push(menu);
}

void refresh_marks(ReaderViewState &state)
{
    std::vector<DocAddr> marks;
    for (const Bookmark &bookmark : state.bookmarks)
    {
        marks.push_back(bookmark.address);
    }
    state.token_view->set_marks(marks);
}

void push_undo(ReaderViewState &state, UndoEntry entry)
{
    if (entry.highlights.empty() && entry.bookmarks.empty() && entry.words.empty())
    {
        return;
    }
    std::string what;
    const auto part = [&what](size_t count, const char *one, const char *many) {
        if (count == 0)
        {
            return;
        }
        what += (what.empty() ? "" : ", ") + std::to_string(count) + " " + (count == 1 ? one : many);
    };
    part(entry.highlights.size(), "highlight", "highlights");
    part(entry.bookmarks.size(), "bookmark", "bookmarks");
    part(entry.words.size(), "word", "words");
    entry.what = what;
    state.undo.push_back(std::move(entry));
    if (state.undo.size() > 20)
    {
        state.undo.erase(state.undo.begin());
    }
}

// Remembers the highlights that are gone or have lost their note since the snapshot was taken.
void remember_highlight_changes(ReaderViewState &state, const std::vector<Highlight> &before)
{
    const auto &now = state.token_view->highlights();
    UndoEntry entry;
    for (const Highlight &old : before)
    {
        const auto found = std::find_if(now.begin(), now.end(), [&old](const Highlight &h) {
            return h.first == old.first && h.second == old.second;
        });
        if (found == now.end() || (found->note != old.note && !old.note.empty()))
        {
            entry.highlights.push_back(old);
        }
    }
    push_undo(state, std::move(entry));
}

// Highlight, underline or strike out the selection, or take it off, keeping what goes for Undo.
void toggle_highlight_with_undo(ReaderViewState &state, HighlightStyle style)
{
    const std::vector<Highlight> before = state.token_view->highlights();
    state.token_view->toggle_selected_highlight(style);
    remember_highlight_changes(state, before);
}

void undo_last(ReaderViewState &state)
{
    if (state.undo.empty())
    {
        show_message(state, "Nothing to undo");
        return;
    }
    UndoEntry entry = std::move(state.undo.back());
    state.undo.pop_back();
    for (const Highlight &old : entry.highlights)
    {
        const auto &now = state.token_view->highlights();
        size_t at = now.size();
        for (size_t i = 0; i < now.size(); ++i)
        {
            if (now[i].first == old.first && now[i].second == old.second)
            {
                at = i;
                break;
            }
        }
        if (at < now.size())
        {
            state.token_view->set_highlight_note(at, old.note);
        }
        else
        {
            state.token_view->merge_highlights({old});
        }
    }
    if (!entry.bookmarks.empty())
    {
        for (const Bookmark &back : entry.bookmarks)
        {
            const bool there = std::any_of(state.bookmarks.begin(), state.bookmarks.end(), [&back](const Bookmark &b) {
                return b.address == back.address;
            });
            if (!there)
            {
                state.bookmarks.push_back(back);
            }
        }
        std::stable_sort(state.bookmarks.begin(), state.bookmarks.end(), [](const Bookmark &a, const Bookmark &b) {
            return a.address < b.address;
        });
        save_bookmarks(state.bookmarks_path, state.bookmarks);
        refresh_marks(state);
    }
    for (const UndoEntry::Word &word : entry.words)
    {
        vocabulary::add(word.entry.word, word.entry.gloss, word.entry.book, word.entry.when, word.sentence);
    }
    show_message(state, "Brought back " + entry.what);
}

void add_bookmark(ReaderViewState &state)
{
    const DocAddr address = get_current_address(state);
    for (const Bookmark &bookmark : state.bookmarks)
    {
        if (bookmark.address == address)
        {
            show_message(state, "Already bookmarked");
            return;
        }
    }

    const auto &toc = state.reader->get_table_of_contents();
    const auto position = state.reader->get_toc_position(address);
    std::string name = position.toc_index < toc.size() ? toc[position.toc_index].display_name : state.book_title;
    state.bookmarks.push_back({address, name});
    std::stable_sort(state.bookmarks.begin(), state.bookmarks.end(), [](const Bookmark &a, const Bookmark &b) {
        return a.address < b.address;
    });
    save_bookmarks(state.bookmarks_path, state.bookmarks);
    refresh_marks(state);
    show_message(state, "Bookmark added");
}

// True when the page on screen holds a bookmark.
bool page_has_bookmark(ReaderViewState &state)
{
    DocAddr from = 0;
    DocAddr until = 0;
    return state.token_view->visible_range(from, until) && bookmark_in_range(state.bookmarks, from, until);
}

// Removes the bookmark on this page, or adds one when there is none.
void toggle_bookmark(ReaderViewState &state)
{
    DocAddr from = 0;
    DocAddr until = 0;
    if (state.token_view->visible_range(from, until) && bookmark_in_range(state.bookmarks, from, until))
    {
        {
            UndoEntry gone;
            for (const Bookmark &b : state.bookmarks)
            {
                if (b.address >= from && b.address < until)
                {
                    gone.bookmarks.push_back(b);
                }
            }
            push_undo(state, std::move(gone));
        }
        state.bookmarks.erase(
            std::remove_if(state.bookmarks.begin(), state.bookmarks.end(), [from, until](const Bookmark &b) {
                return b.address >= from && b.address < until;
            }),
            state.bookmarks.end()
        );
        save_bookmarks(state.bookmarks_path, state.bookmarks);
        refresh_marks(state);
        show_message(state, "Bookmark removed");
        return;
    }
    add_bookmark(state);
}

// Goes to the next or previous bookmark or highlight in book order.
void go_to_mark(ReaderView &reader_view, ReaderViewState &state, bool forward)
{
    std::vector<DocAddr> marks;
    for (const Bookmark &bookmark : state.bookmarks)
    {
        marks.push_back(bookmark.address);
    }
    for (const Highlight &highlight : state.token_view->highlights())
    {
        marks.push_back(highlight.first);
    }
    if (marks.empty())
    {
        show_message(state, "No bookmarks or highlights yet");
        return;
    }
    std::sort(marks.begin(), marks.end());

    // Landing on a mark leaves the view a little before it, so measure from the
    // mark itself while the view has not moved since.
    const DocAddr here = get_current_address(state);
    const DocAddr from = (state.landed_mark && here == state.landed_view) ? *state.landed_mark : here;
    std::optional<DocAddr> target;
    if (forward)
    {
        const auto it = std::upper_bound(marks.begin(), marks.end(), from);
        if (it != marks.end())
        {
            target = *it;
        }
    }
    else
    {
        const auto it = std::lower_bound(marks.begin(), marks.end(), from);
        if (it != marks.begin())
        {
            target = *(it - 1);
        }
    }
    if (!target)
    {
        show_message(state, forward ? "No later bookmark or highlight" : "No earlier bookmark or highlight");
        return;
    }
    reader_view.jump_to(*target);
    state.landed_mark = *target;
    state.landed_view = get_current_address(state);
}

// Opens one of the book's pictures full screen.
void open_picture(ReaderViewState &state, const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    if (!extension.empty() && extension[0] == '.')
    {
        extension.erase(0, 1);
    }
    auto view = std::make_shared<ImageView>(state.reader->load_resource(path), extension, state.sys_styling, state.token_view_styling.get_picture_mode());
    if (!view->valid())
    {
        show_message(state, "This picture cannot be shown");
        return;
    }
    state.view_stack.push(view);
}

// The pictures on the page: opens the only one, or lists them to choose from.
void open_pictures(ReaderViewState &state, const std::vector<std::filesystem::path> &pictures)
{
    if (pictures.size() == 1)
    {
        open_picture(state, pictures[0]);
        return;
    }
    std::vector<std::string> names;
    for (size_t i = 0; i < pictures.size(); ++i)
    {
        names.push_back("Picture " + std::to_string(i + 1) + " of " + std::to_string(pictures.size()));
    }
    auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
    menu->set_title("Pictures");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&state, pictures](uint32_t index) {
        if (index < pictures.size())
        {
            open_picture(state, pictures[index]);
        }
    });
    state.view_stack.push(menu);
}

// The shoulder pair that does not turn pages moves between chapters: back to the start of
// this chapter, or to the one before when already near its start, and forward to the next.
void jump_chapter(ReaderView &reader_view, ReaderViewState &state, int direction)
{
    const auto &toc = state.reader->get_table_of_contents();
    if (toc.empty())
    {
        show_message(state, "This book has no chapters");
        return;
    }
    const DocAddr here = get_current_address(state);
    const auto position = state.reader->get_toc_position(here);
    int target = static_cast<int>(position.toc_index);
    if (direction > 0)
    {
        ++target;
    }
    else if (position.progress_percent < 3)
    {
        --target;
    }
    if (target < 0 || target >= static_cast<int>(toc.size()))
    {
        show_message(state, direction > 0 ? "This is the last chapter" : "This is the first chapter");
        return;
    }
    reader_view.jump_to(state.reader->get_toc_item_address(static_cast<uint32_t>(target)));
}

void open_bookmarks_menu(ReaderView &reader_view, ReaderViewState &state)
{
    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    auto fill = [&state](SelectionMenu &m) {
        std::vector<std::string> names;
        std::vector<std::string> details;
        for (const Bookmark &bookmark : state.bookmarks)
        {
            names.push_back(bookmark.label.empty() ? std::string("Bookmark") : bookmark.label);
            details.push_back(percent_label(state, bookmark.address));
        }
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_title("Bookmarks");
    menu->set_empty_message("No bookmarks yet\nChoose Add bookmark in the book menu");
    menu->set_hints({"A  Go", "Y  Rename", "X  Delete", "B  Close"});
    menu->set_uses_x(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state](uint32_t index) {
        if (index < state.bookmarks.size())
        {
            reader_view.jump_to(state.bookmarks[index].address);
        }
    });
    std::weak_ptr<SelectionMenu> weak_bookmarks = menu;
    menu->set_default_on_keypress([&state, fill, weak_bookmarks](SDLKey key, SelectionMenu &m) {
        if (key == SW_BTN_Y)
        {
            const uint32_t at = m.get_cursor_pos();
            if (at >= state.bookmarks.size())
            {
                return;
            }
            const DocAddr address = state.bookmarks[at].address;
            state.view_stack.push(std::make_shared<TextInputView>(
                "Bookmark name",
                state.bookmarks[at].label,
                [&state, address, at, fill, weak_bookmarks](const std::string &typed) {
                    std::string label = typed;
                    while (!label.empty() && label.back() == ' ') label.pop_back();
                    while (!label.empty() && label.front() == ' ') label.erase(0, 1);
                    if (label.empty())
                    {
                        return;
                    }
                    for (Bookmark &bookmark : state.bookmarks)
                    {
                        if (bookmark.address == address)
                        {
                            bookmark.label = label;
                        }
                    }
                    save_bookmarks(state.bookmarks_path, state.bookmarks);
                    if (auto open = weak_bookmarks.lock())
                    {
                        fill(*open);
                        open->set_cursor_pos(at);
                    }
                },
                state.sys_styling,
                80
            ));
            return;
        }
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= state.bookmarks.size())
        {
            return;
        }
        {
            UndoEntry gone;
            gone.bookmarks.push_back(state.bookmarks[index]);
            push_undo(state, std::move(gone));
        }
        state.bookmarks.erase(state.bookmarks.begin() + index);
        save_bookmarks(state.bookmarks_path, state.bookmarks);
        refresh_marks(state);
        fill(m);
        m.set_cursor_pos(std::min<uint32_t>(index, state.bookmarks.empty() ? 0 : static_cast<uint32_t>(state.bookmarks.size()) - 1));
    });
    state.view_stack.push(menu);
}

// Indexes of the highlights in the order they come in the book.
std::vector<size_t> highlight_order(const std::vector<Highlight> &highlights)
{
    std::vector<size_t> order(highlights.size());
    for (size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&highlights](size_t a, size_t b) {
        return highlights[a].first < highlights[b].first;
    });
    return order;
}

// The kinds the highlight list can be narrowed to: 0 all, 1 with a note, 2 to 5 each color, 6 underlines, 7 strikeouts.
constexpr int HIGHLIGHT_FILTER_COUNT = 8;

bool highlight_in_filter(const Highlight &highlight, int filter)
{
    switch (filter)
    {
        case 1: return !highlight.note.empty();
        case 2: case 3: case 4: case 5: return marker_color_of(highlight.style) == filter - 2;
        case 6: return highlight.style == HighlightStyle::Underline;
        case 7: return highlight.style == HighlightStyle::Strikeout;
        default: return true;
    }
}

std::string highlight_filter_name(int filter)
{
    switch (filter)
    {
        case 1: return "with notes";
        case 2: case 3: case 4: case 5: return std::string(highlight_color_name(filter - 2)) + " markers";
        case 6: return "underlines";
        case 7: return "strikeouts";
        default: return "";
    }
}

void open_highlights_menu(ReaderView &reader_view, ReaderViewState &state)
{
    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    auto order = std::make_shared<std::vector<size_t>>();
    auto filter = std::make_shared<int>(0);
    auto fill = [&state, order, filter](SelectionMenu &m) {
        const auto &highlights = state.token_view->highlights();
        std::vector<size_t> all = highlight_order(highlights);
        if (state.highlights_by_date)
        {
            // The newest first; the ones with no date, made before dates were kept, last in book order.
            std::stable_sort(all.begin(), all.end(), [&highlights](size_t a, size_t b) {
                return highlights[a].made > highlights[b].made;
            });
        }
        order->clear();
        for (const size_t index : all)
        {
            if (highlight_in_filter(highlights[index], *filter))
            {
                order->push_back(index);
            }
        }
        m.set_title(*filter == 0 ? std::string("Highlights") : "Highlights: " + highlight_filter_name(*filter)
            + " (" + std::to_string(order->size()) + " of " + std::to_string(all.size()) + ")");
        std::vector<std::string> names;
        std::vector<std::string> details;
        for (size_t index : *order)
        {
            const Highlight &highlight = highlights[index];
            names.push_back(std::string(highlight.note.empty() ? "" : "* ") + (highlight.text.empty() ? std::string("(text not saved)") : highlight.text));
            std::string detail = percent_label(state, highlight.first);
            if (highlight.made > 0)
            {
                char day[16];
                const time_t made = static_cast<time_t>(highlight.made);
                if (std::strftime(day, sizeof(day), "%Y-%m-%d", std::localtime(&made)) > 0)
                {
                    detail += std::string("  ") + day;
                }
            }
            details.push_back(detail);
        }
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_empty_message("None of this kind\nStart shows another kind");
    menu->set_hints({"A  Go", "Y  Note", "X  Delete", "Start  Filter", "B  Close"});
    menu->set_uses_x(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, order](uint32_t index) {
        if (index < order->size())
        {
            reader_view.jump_to(state.token_view->highlights()[(*order)[index]].first);
        }
    });
    std::weak_ptr<SelectionMenu> weak_menu = menu;
    menu->set_default_on_keypress([&state, order, fill, weak_menu, filter](SDLKey key, SelectionMenu &m) {
        if (key == SW_BTN_START)
        {
            // The next kind that has something in it, then all again.
            const auto &highlights = state.token_view->highlights();
            for (int step = 1; step <= HIGHLIGHT_FILTER_COUNT; ++step)
            {
                const int next = (*filter + step) % HIGHLIGHT_FILTER_COUNT;
                const bool any = next == 0 || std::any_of(highlights.begin(), highlights.end(), [next](const Highlight &h) {
                    return highlight_in_filter(h, next);
                });
                if (any)
                {
                    *filter = next;
                    break;
                }
            }
            fill(m);
            m.set_cursor_pos(0u);
            return;
        }
        if (key == SW_BTN_Y)
        {
            const uint32_t at = m.get_cursor_pos();
            if (at >= order->size())
            {
                return;
            }
            const size_t which = (*order)[at];
            state.view_stack.push(std::make_shared<TextInputView>(
                "Note (empty removes it)",
                state.token_view->highlights()[which].note,
                [&state, which, at, order, fill, weak_menu](const std::string &typed) {
                    std::string note = typed;
                    while (!note.empty() && note.back() == ' ') note.pop_back();
                    while (!note.empty() && note.front() == ' ') note.erase(0, 1);
                    const std::vector<Highlight> before = state.token_view->highlights();
                    state.token_view->set_highlight_note(which, note);
                    remember_highlight_changes(state, before);
                    if (auto open = weak_menu.lock())
                    {
                        fill(*open);
                        open->set_cursor_pos(at);
                    }
                },
                state.sys_styling,
                300
            ));
            return;
        }
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= order->size())
        {
            return;
        }
        {
            const std::vector<Highlight> before = state.token_view->highlights();
            state.token_view->remove_highlight((*order)[index]);
            remember_highlight_changes(state, before);
        }
        fill(m);
        m.set_cursor_pos(std::min<uint32_t>(index, order->empty() ? 0 : static_cast<uint32_t>(order->size()) - 1));
    });
    state.view_stack.push(menu);
}

// Words typed by the reader, found in the names of bookmarks and in highlights and their notes.
void open_marks_search(ReaderView &reader_view, ReaderViewState &state)
{
    state.view_stack.push(std::make_shared<TextInputView>(
        "Search marks and notes",
        "",
        [&reader_view, &state](const std::string &typed) {
            const auto simple = [](const std::string &text) { return lookup_lowercase(fold_accents(text)); };
            const std::string needle = simple(typed);
            if (needle.empty())
            {
                return;
            }
            struct Hit
            {
                DocAddr address;
                std::string name;
                std::string detail;
            };
            auto hits = std::make_shared<std::vector<Hit>>();
            for (const Bookmark &bookmark : state.bookmarks)
            {
                if (simple(bookmark.label).find(needle) != std::string::npos)
                {
                    hits->push_back({bookmark.address, "Bookmark: " + bookmark.label, percent_label(state, bookmark.address)});
                }
            }
            for (const Highlight &highlight : state.token_view->highlights())
            {
                const bool in_text = simple(highlight.text).find(needle) != std::string::npos;
                const bool in_note = simple(highlight.note).find(needle) != std::string::npos;
                if (in_text || in_note)
                {
                    hits->push_back({
                        highlight.first,
                        in_text ? std::string(highlight.note.empty() ? "" : "* ") + highlight.text : "Note: " + highlight.note,
                        percent_label(state, highlight.first)
                    });
                }
            }
            if (hits->empty())
            {
                show_message(state, "No mark or note has \"" + typed + "\"");
                return;
            }
            std::stable_sort(hits->begin(), hits->end(), [](const Hit &a, const Hit &b) { return a.address < b.address; });
            std::vector<std::string> names;
            std::vector<std::string> details;
            for (const Hit &hit : *hits)
            {
                names.push_back(hit.name);
                details.push_back(hit.detail);
            }
            auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
            menu->set_details(details);
            menu->set_title("Marks with \"" + typed + "\"");
            menu->set_hints({"A  Go", "B  Close"});
            menu->set_close_on_select();
            menu->set_on_selection([&reader_view, hits](uint32_t index) {
                if (index < hits->size())
                {
                    reader_view.jump_to((*hits)[index].address);
                }
            });
            state.view_stack.push(menu);
        },
        state.sys_styling,
        60
    ));
}

// The places you jumped away from, the latest first.
void open_places_menu(ReaderView &reader_view, ReaderViewState &state)
{
    auto places = std::make_shared<std::vector<DocAddr>>(state.jump_stack.rbegin(), state.jump_stack.rend());
    if (places->empty())
    {
        show_message(state, "No places yet");
        return;
    }
    const auto &toc = state.reader->get_table_of_contents();
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const DocAddr address : *places)
    {
        std::string chapter;
        if (!toc.empty())
        {
            const uint32_t index = state.reader->get_toc_position(address).toc_index;
            if (index < toc.size())
            {
                chapter = toc[index].display_name;
            }
        }
        names.push_back(chapter.empty() ? std::string("Place in the book") : chapter);
        details.push_back(percent_label(state, address));
    }
    auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
    menu->set_details(details);
    menu->set_title("Places you jumped from");
    menu->set_hints({"A  Go", "B  Close"});
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, places](uint32_t index) {
        if (index < places->size())
        {
            reader_view.jump_to((*places)[index]);
        }
    });
    state.view_stack.push(menu);
}

// The words looked up, newest first; with this_book_only just the ones looked up in this book, each with the
// sentence it was found in.
void open_word_list(ReaderViewState &state, bool this_book_only = false)
{
    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    auto order = std::make_shared<std::vector<size_t>>(); // the entry behind each row
    const std::string book_title = state.book_title;
    // The first 70 letters of a sentence on one line.
    const auto shorten = [](const std::string &text) {
        std::string line;
        size_t letters = 0;
        for (size_t i = 0; i < text.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if ((c & 0xC0) != 0x80)
            {
                if (letters >= 70)
                {
                    return line + "...";
                }
                ++letters;
            }
            line.push_back(c == '\n' || c == '\t' ? ' ' : static_cast<char>(c));
        }
        return line;
    };
    auto fill = [order, this_book_only, book_title, shorten](SelectionMenu &m) {
        std::vector<std::string> names;
        std::vector<std::string> details;
        order->clear();
        const auto &list = vocabulary::entries();
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (this_book_only && list[i].book != book_title)
            {
                continue;
            }
            order->push_back(i);
            names.push_back(list[i].word);
            details.push_back(this_book_only ? shorten(vocabulary::context_of(list[i].word)) : list[i].book);
        }
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_title(this_book_only ? "Words of this book" : "Word list");
    menu->set_empty_message(this_book_only ? "No words from this book yet\nLook a word up and it is kept here" : "No words yet\nLook a word up and it is kept here");
    menu->set_hints({"A  Meaning", "X  Delete", "B  Close"});
    menu->set_uses_x(true);
    menu->set_on_selection([&state, order](uint32_t index) {
        const auto &list = vocabulary::entries();
        if (index < order->size() && (*order)[index] < list.size())
        {
            const vocabulary::Entry &entry = list[(*order)[index]];
            const std::string sentence = vocabulary::context_of(entry.word);
            state.view_stack.push(std::make_shared<DefinitionView>(
                entry.word,
                entry.gloss + (sentence.empty() ? std::string() : "\n\nIn the book: " + sentence)
                    + (entry.book.empty() ? std::string() : "\n\nFrom: " + entry.book),
                state.sys_styling
            ));
        }
    });
    menu->set_default_on_keypress([&state, fill, order](SDLKey key, SelectionMenu &m) {
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= order->size() || (*order)[index] >= vocabulary::entries().size())
        {
            return;
        }
        {
            const vocabulary::Entry &entry = vocabulary::entries()[(*order)[index]];
            UndoEntry gone;
            gone.words.push_back({entry, vocabulary::context_of(entry.word)});
            push_undo(state, std::move(gone));
        }
        vocabulary::remove((*order)[index]);
        fill(m);
        const size_t left = order->size();
        m.set_cursor_pos(std::min<uint32_t>(index, left == 0 ? 0 : static_cast<uint32_t>(left) - 1));
    });
    state.view_stack.push(menu);
}

// Goes through the words that are due, one card at a time.
void review_words(ReaderViewState &state)
{
    const long long now = static_cast<long long>(std::time(nullptr));
    auto queue = std::make_shared<std::deque<std::string>>();
    for (const std::string &word : vocabulary::due_words(now))
    {
        if (queue->size() < 30)
        {
            queue->push_back(word);
        }
    }
    if (queue->empty())
    {
        show_message(state, "Nothing to review right now");
        return;
    }
    auto total = std::make_shared<size_t>(queue->size());
    auto retried = std::make_shared<std::set<std::string>>();
    const auto card_for = [queue, total](const std::string &word) -> std::pair<std::string, std::string> {
        std::string meaning;
        std::string book;
        for (const auto &entry : vocabulary::entries())
        {
            if (entry.word == word)
            {
                meaning = entry.gloss;
                book = entry.book;
                break;
            }
        }
        const size_t done = *total - std::min(*total, queue->size()) + 1;
        return {
            word + "   (" + std::to_string(done) + " / " + std::to_string(*total) + ")",
            meaning + (vocabulary::context_of(word).empty() ? std::string() : "\n\n\"" + vocabulary::context_of(word) + "\"")
                + (book.empty() ? std::string() : "\n\nFrom: " + book)
        };
    };
    const auto first = card_for(queue->front());
    auto view = std::make_shared<DefinitionView>(first.first, std::string(), state.sys_styling);
    view->set_card(first.second, [queue, total, retried, card_for](bool known) -> std::optional<std::pair<std::string, std::string>> {
        const std::string word = queue->front();
        queue->pop_front();
        vocabulary::grade(word, known, static_cast<long long>(std::time(nullptr)));
        if (!known && retried->insert(word).second)
        {
            // Once more at the end of this session
            queue->push_back(word);
            ++*total;
        }
        if (queue->empty())
        {
            return std::nullopt;
        }
        return card_for(queue->front());
    });
    state.view_stack.push(view);
}

// Writes the word list as a file Anki can import: one card for each word, with the meaning, the
// sentence it was found in and the book as a tag.
void export_anki(ReaderViewState &state)
{
    const auto &list = vocabulary::entries();
    if (list.empty())
    {
        show_message(state, "No words to export");
        return;
    }
    const auto html = [](const std::string &text) {
        std::string out;
        for (const char c : text)
        {
            if (c == '&') out += "&amp;";
            else if (c == '<') out += "&lt;";
            else if (c == '>') out += "&gt;";
            else if (c == '\n') out += "<br>";
            else if (c == '\t' || c == '\r') out += ' ';
            else out += c;
        }
        return out;
    };
    const std::filesystem::path out_path = state.book_path.parent_path() / "Anki words.txt";
    std::ofstream out(out_path, std::ios::trunc);
    out << "#separator:tab\n#html:true\n#tags column:3\n";
    for (auto it = list.rbegin(); it != list.rend(); ++it)
    {
        std::string back = html(it->gloss);
        const std::string sentence = vocabulary::context_of(it->word);
        if (!sentence.empty())
        {
            back += "<br><br><i>" + html(sentence) + "</i>";
        }
        std::string tag = it->book;
        for (char &c : tag)
        {
            if (c == ' ' || c == '\t') c = '_';
        }
        out << html(it->word) << '\t' << back << '\t' << tag << '\n';
    }
    out.close();
    if (!out)
    {
        show_message(state, "Could not write the file");
        return;
    }
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Anki file written",
        "Saved " + std::to_string(list.size()) + " cards to\n" + out_path.string()
            + "\n\nIn Anki choose File, Import. The word is the front, the meaning and the sentence the back, the book a tag.",
        state.sys_styling
    ));
}

// Writes the word list next to the book as a text file.
void export_word_list(ReaderViewState &state)
{
    const auto &list = vocabulary::entries();
    if (list.empty())
    {
        show_message(state, "No words to export");
        return;
    }
    const std::filesystem::path out_path = state.book_path.parent_path() / "Word list.txt";
    std::ofstream out(out_path, std::ios::trunc);
    out << "Word list\n\n";
    for (auto it = list.rbegin(); it != list.rend(); ++it)
    {
        out << it->word << " - " << it->gloss;
        if (!it->book.empty())
        {
            out << "  (" << it->book << ")";
        }
        out << "\n\n";
    }
    out.close();
    if (!out)
    {
        show_message(state, "Could not write the file");
        return;
    }
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Word list exported",
        "Saved " + std::to_string(list.size()) + " words to\n" + out_path.string(),
        state.sys_styling
    ));
}

// The name of the Markdown file of highlights next to the book.
std::filesystem::path highlights_file_for(const ReaderViewState &state)
{
    std::string name = state.book_title;
    for (char &c : name)
    {
        if (std::string("/\\:*?\"<>|").find(c) != std::string::npos)
        {
            c = '_';
        }
    }
    return state.book_path.parent_path() / (name + " - highlights.md");
}

// The part of the book the chapter at the address covers, [from, until). All of it when there are no chapters.
std::pair<DocAddr, DocAddr> chapter_bounds(ReaderViewState &state, DocAddr address)
{
    const DocAddr end = std::numeric_limits<DocAddr>::max();
    const auto &toc = state.reader->get_table_of_contents();
    if (toc.empty())
    {
        return {0, end};
    }
    const uint32_t index = state.reader->get_toc_position(address).toc_index;
    if (index >= toc.size())
    {
        return {0, end};
    }
    const DocAddr from = state.reader->get_toc_item_address(index);
    DocAddr until = end;
    for (uint32_t next = index + 1; next < toc.size(); ++next)
    {
        const DocAddr found = state.reader->get_toc_item_address(next);
        if (found > from)
        {
            until = found;
            break;
        }
    }
    return {from, until};
}

// Writes the highlights and their notes next to the book as a Markdown file. Each one ends with a
// comment holding its place, so the file can be read back into the book later. With this_chapter only
// the highlights of the chapter being read go in, in a file of their own named after it.
void export_highlights(ReaderViewState &state, bool this_chapter = false)
{
    const auto &highlights = state.token_view->highlights();
    if (highlights.empty())
    {
        show_message(state, "No highlights to export");
        return;
    }

    std::pair<DocAddr, DocAddr> bounds = {0, std::numeric_limits<DocAddr>::max()};
    std::string chapter_name;
    if (this_chapter)
    {
        const DocAddr here = get_current_address(state);
        bounds = chapter_bounds(state, here);
        const auto &chapters = state.reader->get_table_of_contents();
        const uint32_t at = state.reader->get_toc_position(here).toc_index;
        chapter_name = at < chapters.size() ? chapters[at].display_name : std::string("this chapter");
    }
    std::vector<size_t> chosen;
    for (size_t index : highlight_order(highlights))
    {
        if (highlights[index].first >= bounds.first && highlights[index].first < bounds.second)
        {
            chosen.push_back(index);
        }
    }
    if (chosen.empty())
    {
        show_message(state, "No highlights in this chapter");
        return;
    }

    std::filesystem::path out_path = highlights_file_for(state);
    if (this_chapter)
    {
        std::string part = chapter_name;
        for (char &c : part)
        {
            if (std::string("/\\:*?\"<>|").find(c) != std::string::npos)
            {
                c = '_';
            }
        }
        out_path = out_path.parent_path() / (out_path.stem().string() + " - " + part + ".md");
    }
    const auto &toc = state.reader->get_table_of_contents();
    std::ofstream out(out_path, std::ios::trunc);
    out << "# " << state.book_title << "\n\n## Highlights" << (this_chapter ? ": " + chapter_name : std::string()) << "\n\n";
    std::string last_chapter;
    for (size_t index : chosen)
    {
        const Highlight &highlight = highlights[index];
        const auto position = state.reader->get_toc_position(highlight.first);
        if (position.toc_index < toc.size() && toc[position.toc_index].display_name != last_chapter)
        {
            last_chapter = toc[position.toc_index].display_name;
            out << "### " << last_chapter << "\n\n";
        }
        const int color = marker_color_of(highlight.style);
        out << "> " << (highlight.text.empty() ? std::string("(text not saved)") : highlight.text) << "\n\n";
        if (!highlight.note.empty())
        {
            out << "**Note:** " << highlight.note << "\n\n";
        }
        std::string made_day;
        if (highlight.made > 0)
        {
            char day[16];
            const time_t made = static_cast<time_t>(highlight.made);
            if (std::strftime(day, sizeof(day), "%Y-%m-%d", std::localtime(&made)) > 0)
            {
                made_day = std::string(", ") + day;
            }
        }
        out << "*" << percent_label(state, highlight.first)
            << (color > 0 ? std::string(", ") + highlight_color_name(color) : std::string()) << made_day << "*\n";
        out << "<!-- px " << highlight.first << " " << highlight.second << " " << highlight_style_letter(highlight.style) << " -->\n\n";
    }
    out.close();

    if (!out)
    {
        show_message(state, "Could not write the file");
        return;
    }
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Highlights exported",
        "Saved " + std::to_string(chosen.size()) + " highlights and notes to\n" + out_path.string()
            + "\n\nImport highlights reads the file back into this book, on this or another copy of it.",
        state.sys_styling
    ));
}

// Changes to many highlights at once: this chapter's, or all of one kind.
void open_highlight_tools(ReaderViewState &state)
{
    const auto &highlights = state.token_view->highlights();
    const auto bounds = chapter_bounds(state, get_current_address(state));
    size_t in_chapter = 0;
    for (const Highlight &highlight : highlights)
    {
        if (highlight.first >= bounds.first && highlight.first < bounds.second)
        {
            ++in_chapter;
        }
    }
    std::vector<std::string> names;
    std::vector<std::function<void()>> runs;
    names.push_back(state.highlights_by_date ? "List highlights in book order" : "List highlights newest first");
    runs.push_back([&state]() {
        state.highlights_by_date = !state.highlights_by_date;
        show_message(state, state.highlights_by_date ? "The highlight list shows the newest first" : "The highlight list is in book order");
    });
    if (in_chapter > 0)
    {
        names.push_back("Export this chapter's highlights (" + std::to_string(in_chapter) + ")");
        runs.push_back([&state]() { export_highlights(state, true); });
        names.push_back("Delete this chapter's highlights (" + std::to_string(in_chapter) + ")");
        runs.push_back([&state, bounds, in_chapter]() {
            auto confirm = std::make_shared<SelectionMenu>(std::vector<std::string> {"Keep them", "Delete them"}, state.sys_styling);
            confirm->set_title("Delete " + std::to_string(in_chapter) + (in_chapter == 1 ? " highlight of this chapter?" : " highlights of this chapter?"));
            confirm->set_compact(true);
            confirm->set_close_on_select();
            confirm->set_on_selection([&state, bounds](uint32_t choice) {
                if (choice != 1)
                {
                    return;
                }
                size_t removed = 0;
                const std::vector<Highlight> before = state.token_view->highlights();
                for (size_t index = state.token_view->highlights().size(); index-- > 0;)
                {
                    const DocAddr first = state.token_view->highlights()[index].first;
                    if (first >= bounds.first && first < bounds.second)
                    {
                        state.token_view->remove_highlight(index);
                        ++removed;
                    }
                }
                remember_highlight_changes(state, before);
                show_message(state, "Deleted " + std::to_string(removed) + (removed == 1 ? " highlight" : " highlights"));
            });
            state.view_stack.push(confirm);
        });
    }
    // The kinds in use: 2 to 5 the four colors, 6 underlines, 7 strikeouts, as the highlight list names them.
    std::vector<int> kinds;
    for (int kind = 2; kind < HIGHLIGHT_FILTER_COUNT; ++kind)
    {
        size_t count = 0;
        for (const Highlight &highlight : highlights)
        {
            count += highlight_in_filter(highlight, kind) ? 1 : 0;
        }
        if (count > 0)
        {
            kinds.push_back(kind);
            names.push_back("All " + highlight_filter_name(kind) + " (" + std::to_string(count) + ")...");
            runs.push_back([&state, kind, count]() {
                const auto style_of = [](int k) {
                    return k == 6 ? HighlightStyle::Underline : (k == 7 ? HighlightStyle::Strikeout : marker_style(k - 2));
                };
                std::vector<std::string> options;
                std::vector<int> targets;
                for (int target = 2; target < HIGHLIGHT_FILTER_COUNT; ++target)
                {
                    if (target != kind)
                    {
                        options.push_back("Make them " + highlight_filter_name(target).substr(0, highlight_filter_name(target).size() - 1));
                        targets.push_back(target);
                    }
                }
                options.push_back("Delete them");
                auto menu = std::make_shared<SelectionMenu>(options, state.sys_styling);
                menu->set_title(std::to_string(count) + " " + highlight_filter_name(kind));
                menu->set_compact(true);
                menu->set_close_on_select();
                menu->set_on_selection([&state, kind, targets, style_of](uint32_t choice) {
                    size_t changed = 0;
                    if (choice < targets.size())
                    {
                        const HighlightStyle to = style_of(targets[choice]);
                        for (size_t index = 0; index < state.token_view->highlights().size(); ++index)
                        {
                            if (highlight_in_filter(state.token_view->highlights()[index], kind))
                            {
                                state.token_view->set_highlight_style(index, to);
                                ++changed;
                            }
                        }
                        show_message(state, "Changed " + std::to_string(changed) + (changed == 1 ? " highlight" : " highlights"));
                        return;
                    }
                    auto confirm = std::make_shared<SelectionMenu>(std::vector<std::string> {"Keep them", "Delete them"}, state.sys_styling);
                    confirm->set_title("Delete all " + highlight_filter_name(kind) + "?");
                    confirm->set_compact(true);
                    confirm->set_close_on_select();
                    confirm->set_on_selection([&state, kind](uint32_t sure) {
                        if (sure != 1)
                        {
                            return;
                        }
                        size_t removed = 0;
                        const std::vector<Highlight> before = state.token_view->highlights();
                        for (size_t index = state.token_view->highlights().size(); index-- > 0;)
                        {
                            if (highlight_in_filter(state.token_view->highlights()[index], kind))
                            {
                                state.token_view->remove_highlight(index);
                                ++removed;
                            }
                        }
                        remember_highlight_changes(state, before);
                        show_message(state, "Deleted " + std::to_string(removed) + (removed == 1 ? " highlight" : " highlights"));
                    });
                    state.view_stack.push(confirm);
                });
                state.view_stack.push(menu);
            });
        }
    }
    if (names.empty())
    {
        show_message(state, "Nothing to change here");
        return;
    }
    auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
    menu->set_title("Highlight tools");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([runs](uint32_t index) {
        if (index < runs.size())
        {
            runs[index]();
        }
    });
    state.view_stack.push(menu);
}

// Reads the Markdown file written by Export highlights and adds what the book does not have yet.
void import_highlights(ReaderViewState &state)
{
    const std::filesystem::path in_path = highlights_file_for(state);
    std::ifstream in(in_path);
    if (!in)
    {
        show_message(state, "No highlights file next to the book");
        return;
    }
    std::vector<Highlight> found;
    std::string text;
    std::string note;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.rfind("> ", 0) == 0)
        {
            text = line.substr(2);
            if (text == "(text not saved)")
            {
                text.clear();
            }
        }
        else if (line.rfind("**Note:** ", 0) == 0)
        {
            note = line.substr(10);
        }
        else if (line.rfind("<!-- px ", 0) == 0)
        {
            unsigned long long first = 0;
            unsigned long long second = 0;
            char letter = 'y';
            if (std::sscanf(line.c_str(), "<!-- px %llu %llu %c", &first, &second, &letter) >= 2 && second > first)
            {
                found.emplace_back(static_cast<DocAddr>(first), static_cast<DocAddr>(second), text, highlight_style_from_letter(letter));
                found.back().note = note;
            }
            text.clear();
            note.clear();
        }
    }
    if (found.empty())
    {
        show_message(state, "No highlights found in the file");
        return;
    }
    const size_t changed = state.token_view->merge_highlights(found);
    show_message(state, std::to_string(changed) + " added or updated, " + std::to_string(found.size() - std::min(found.size(), changed)) + " already there");
}

} // namespace

void show_definition(ReaderViewState &state, const std::string &lookup, const std::string &chosen = std::string());
static std::map<std::string, Dictionary> &dictionary_cache()
{
    static std::map<std::string, Dictionary> dictionaries;
    return dictionaries;
}
// Lists the words of the dictionary that begin with what is typed; choosing one shows its entry.
static void open_dictionary_browser(ReaderViewState &state);
void open_search_menu(ReaderView &reader_view, ReaderViewState &state);
static void step_match(ReaderView &reader_view, ReaderViewState &state, int direction);
static void open_controls_guide(ReaderViewState &state);

void ReaderView::open_book_menu()
{
    struct Action
    {
        std::string name;
        std::function<void()> run;
        std::string group;
    };
    auto actions = std::make_shared<std::vector<Action>>();
    // The rows are grouped by what they are about. Empty puts a row on the first screen.
    std::string group;
    const auto add = [&actions, &group](Action action) {
        action.group = group;
        actions->push_back(std::move(action));
    };

    if (!state->reader->get_table_of_contents().empty())
    {
        add({"Chapters", [this]() { open_toc_menu(*this, *state); }});
    }
    if (state->search_hits && !state->search_hits->empty())
    {
        const std::string place = std::to_string(state->search_index + 1) + " / " + std::to_string(state->search_hits->size());
        add({"Next match (" + place + ")", [this]() { step_match(*this, *state, 1); }});
        add({"Previous match", [this]() { step_match(*this, *state, -1); }});
        group = "Search";
        add({"Clear search", [this]() {
            state->search_hits.reset();
            state->token_view->set_search_marks("", "");
        }});
    }
    group = "Words";
    add({
        "Book language: " + (state->token_view_styling.get_book_language().empty() ? "auto (" + state->dictionary_name + ")" : state->token_view_styling.get_book_language()),
        [this]() {
            std::vector<std::string> names = {"Follow the book"};
            for (const std::string &name : dictionary_names())
            {
                names.push_back(name);
            }
            auto menu = std::make_shared<SelectionMenu>(names, state->sys_styling);
            menu->set_title("Language of this book");
            menu->set_compact(true);
            menu->set_close_on_select();
            menu->set_hints({"A  Choose", "B  Close"});
            menu->set_on_selection([this, names](uint32_t index) {
                state->token_view_styling.set_book_language(index == 0 || index >= names.size() ? std::string() : names[index]);
            });
            state->view_stack.push(menu);
        }
    });
    group = "Marks";
    add({page_has_bookmark(*state) ? "Remove the bookmark on this page" : "Add bookmark", [this]() { toggle_bookmark(*state); }});
    add({
        "Bookmarks (" + std::to_string(state->bookmarks.size()) + ")",
        [this]() { open_bookmarks_menu(*this, *state); }
    });
    if (!state->bookmarks.empty() || !state->token_view->highlights().empty())
    {
        add({"Next mark", [this]() { go_to_mark(*this, *state, true); }});
        add({"Previous mark", [this]() { go_to_mark(*this, *state, false); }});
    }
    group = "Go to";
    add({"Book map", [this]() {
        std::vector<DocAddr> bookmark_addresses;
        for (const Bookmark &bookmark : state->bookmarks)
        {
            bookmark_addresses.push_back(bookmark.address);
        }
        std::vector<DocAddr> highlight_addresses;
        for (const Highlight &highlight : state->token_view->highlights())
        {
            highlight_addresses.push_back(highlight.first);
        }
        state->view_stack.push(std::make_shared<BookMapView>(
            state->reader,
            get_current_address(*state),
            bookmark_addresses,
            highlight_addresses,
            [this](DocAddr address) { jump_to(address); },
            state->sys_styling
        ));
    }});
    group = "Search";
    add({"Search in book", [this]() { open_search_menu(*this, *state); }});
    group = "Go to";
    add({"Skim the book", [this]() {
        state->view_stack.push(std::make_shared<PageBrowserView>(
            state->reader,
            get_current_address(*state),
            [this](DocAddr address) { jump_to(address); },
            state->sys_styling
        ));
    }});
    add({"Go to %", [this]() {
        const uint32_t now = state->reader->get_global_progress_percent(get_current_address(*state));
        state->view_stack.push(std::make_shared<GotoView>(
            "Go to", "%", 0, 100, now,
            [this](uint32_t percent) { jump_to(state->reader->get_address_at_percent(percent)); },
            state->sys_styling
        ));
    }});
    if (!state->reader->page_list().empty() && !(style_tweaks() & TWEAK_PLAIN_PAGES))
    {
        add({"Go to book page", [this]() {
            const auto &printed = state->reader->page_list();
            state->view_stack.push(std::make_shared<TextInputView>(
                "Go to page (" + printed.front().label + " to " + printed.back().label + ")",
                "",
                [this](const std::string &typed) {
                    std::string wanted = lookup_lowercase(typed);
                    while (!wanted.empty() && wanted.back() == ' ') wanted.pop_back();
                    while (!wanted.empty() && wanted.front() == ' ') wanted.erase(0, 1);
                    for (const PageMark &mark : state->reader->page_list())
                    {
                        if (lookup_lowercase(mark.label) == wanted)
                        {
                            jump_to(mark.address);
                            return;
                        }
                    }
                    show_message(*state, "That page is not in the list");
                },
                state->sys_styling,
                12
            ));
        }});
    }
    {
        const bool chapter = false;
        uint32_t page = 0;
        uint32_t pages = 0;
        if (state->token_view_styling.get_reading_mode() == ReadingMode::Page
            && state->token_view->page_numbers(chapter, page, pages))
        {
            add({"Go to page", [this, chapter, page, pages]() {
                state->view_stack.push(std::make_shared<GotoView>(
                    "Go to page of " + std::to_string(pages), "", 1, pages, page,
                    [this, chapter](uint32_t chosen) { go_to_page(chapter, chosen); },
                    state->sys_styling
                ));
            }});
        }
    }
    group = "Pictures";
    {
        const auto pictures = state->token_view->images_on_screen();
        if (!pictures.empty())
        {
            add({
                pictures.size() == 1 ? std::string("View picture") : "View pictures (" + std::to_string(pictures.size()) + ")",
                [this, pictures]() { open_pictures(*state, pictures); }
            });
        }
    }
    add({"Pictures in this book", [this]() {
        SystemStyling &styling = state->sys_styling;
        std::vector<GalleryPicture> found = find_pictures(*state->reader, 400, [&styling](int percent) {
            progress_screen::show(styling, "Finding pictures", "", percent);
        });
        if (found.empty())
        {
            show_message(*state, "This book has no pictures");
            return;
        }
        ReaderViewState *view_state = state.get();
        state->view_stack.push(std::make_shared<GalleryView>(
            state->reader,
            std::move(found),
            [view_state](const std::filesystem::path &path) { open_picture(*view_state, path); },
            [this](DocAddr address) { jump_to(address); },
            state->sys_styling
        ));
    }});
    group = "Go to";
    if (can_jump_back())
    {
        add({"Jump back", [this]() { jump_back(); }});
        add({"Places you jumped from", [this]() { open_places_menu(*this, *state); }});
    }
    group = "Tools";
    add({"Reset this book's layout", [this]() {
        book_layout::reset();
        show_message(*state, "This book now uses the default layout");
    }});
    add({"Controls guide", [this]() { open_controls_guide(*state); }});
    add({
        std::string("Screen dim: ") + screen_dim::label_for_level(state->token_view_styling.get_screen_dim()),
        [this]() {
            // Darker at each press, and back to normal after the darkest.
            state->token_view_styling.set_screen_dim(
                (state->token_view_styling.get_screen_dim() + 1) % screen_dim::LEVEL_COUNT
            );
        }
    });
    add({"Sync progress", [this]() { open_sync_menu(*this, *state); }});
    group = "Words";
    add({"Look up a word", [this]() {
        const auto type_a_word = [this]() {
            state->view_stack.push(std::make_shared<TextInputView>(
                "Look up a word",
                "",
                [this](const std::string &typed) {
                    std::string word = lookup_lowercase(typed);
                    while (!word.empty() && word.back() == ' ') word.pop_back();
                    while (!word.empty() && word.front() == ' ') word.erase(0, 1);
                    if (!word.empty())
                    {
                        show_definition(*state, word);
                    }
                },
                state->sys_styling,
                60
            ));
        };
        // The words looked up lately, newest first, one press away.
        std::vector<std::string> rows = {"Type a word...", "Browse the dictionary..."};
        std::vector<std::string> recent;
        for (const auto &entry : vocabulary::entries())
        {
            if (recent.size() >= 20)
            {
                break;
            }
            recent.push_back(entry.word);
            rows.push_back(entry.word);
        }
        auto menu = std::make_shared<SelectionMenu>(rows, state->sys_styling);
        menu->set_title("Look up");
        menu->set_compact(true);
        menu->set_close_on_select();
        menu->set_on_selection([this, type_a_word, recent](uint32_t index) {
            if (index == 0)
            {
                type_a_word();
            }
            else if (index == 1)
            {
                open_dictionary_browser(*state);
            }
            else if (index - 2 < recent.size())
            {
                show_definition(*state, recent[index - 2]);
            }
        });
        state->view_stack.push(menu);
    }});
    add({
        "Word list (" + std::to_string(vocabulary::entries().size()) + ")",
        [this]() { open_word_list(*state); }
    });
    {
        size_t from_this_book = 0;
        for (const vocabulary::Entry &entry : vocabulary::entries())
        {
            from_this_book += entry.book == state->book_title ? 1 : 0;
        }
        if (from_this_book > 0)
        {
            add({"Words of this book (" + std::to_string(from_this_book) + ")", [this]() { open_word_list(*state, true); }});
        }
    }
    if (!vocabulary::entries().empty())
    {
        const size_t due = vocabulary::due_words(static_cast<long long>(std::time(nullptr))).size();
        add({"Review words (" + std::to_string(due) + " due)", [this]() { review_words(*state); }});
        add({"Export word list", [this]() { export_word_list(*state); }});
        add({"Export for Anki", [this]() { export_anki(*state); }});
    }
    group = "Tools";
    add({"Reading history", [this]() {
        state->view_stack.push(std::make_shared<ReadingHistoryView>(
            state->sys_styling,
            [](const std::string &path) { return std::filesystem::path(path).stem().string(); }
        ));
    }});
    add({"Reading stats", [this]() {
        state->view_stack.push(std::make_shared<DefinitionView>(
            "Reading stats",
            [this]() {
                std::vector<long long> finished_times;
                for (const auto &entry : library_log().all())
                {
                    if (entry.second.finished())
                    {
                        finished_times.push_back(entry.second.last_opened);
                    }
                }
                const auto extent = state->reader->get_progress_extent(get_current_address(*state), false);
                return reading_stats::detailed_report(
                    state->book_path,
                    static_cast<long long>(std::time(nullptr)),
                    library_log().started_count(),
                    library_log().finished_count(),
                    finished_times,
                    static_cast<long long>(extent.first),
                    [](const std::string &path) { return std::filesystem::path(path).stem().string(); }
                );
            }(),
            state->sys_styling
        ));
    }});
    group = "Marks";
    if (!state->undo.empty())
    {
        add({"Undo delete (" + state->undo.back().what + ")", [this]() { undo_last(*state); }});
    }
    add({
        "Highlights (" + std::to_string(state->token_view->highlights().size()) + ")",
        [this]() { open_highlights_menu(*this, *state); }
    });
    if (!state->bookmarks.empty() || !state->token_view->highlights().empty())
    {
        add({"Search marks and notes", [this]() { open_marks_search(*this, *state); }});
    }
    if (!state->token_view->highlights().empty())
    {
        add({"Export highlights and notes", [this]() { export_highlights(*state); }});
        add({"Highlight tools", [this]() { open_highlight_tools(*state); }});
    }
    {
        std::error_code ec;
        if (std::filesystem::exists(highlights_file_for(*state), ec))
        {
            add({"Import highlights", [this]() { import_highlights(*state); }});
        }
    }

    std::vector<GroupedEntry> entries;
    for (const Action &action : *actions)
    {
        entries.push_back({action.name, "", action.group});
    }
    open_grouped_menu(
        [this](std::shared_ptr<View> view) { state->view_stack.push(view); },
        state->sys_styling,
        "Book",
        std::move(entries),
        [actions](uint32_t index) {
            if (index < actions->size())
            {
                (*actions)[index].run();
            }
        },
        {"Go to", "Search", "Marks", "Words", "Pictures", "Tools"},
        SW_BTN_SELECT,
        [this]() { return state->view_stack.top_view(); }
    );
}

ReaderView::ReaderView(
    std::filesystem::path path,
    std::shared_ptr<DocReader> reader,
    DocAddr seek_address,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    ViewStack &view_stack,
    std::filesystem::path highlights_path
) : state(std::make_unique<ReaderViewState>(
        path,
        seek_address,
        reader,
        sys_styling,
        token_view_styling,
        token_view_styling.subscribe_to_changes([this]() {
            apply_book_language(*state);
            update_token_view_title(get_current_address(*state));
        }),
        view_stack,
        std::move(highlights_path)
    ))
{
    reading_stats::reset();
    const auto book_info = read_epub_info(path);
    state->book_title = book_display_title(book_info, path);
    if (book_info)
    {
        state->dictionary_name = dictionary_name_for_language(book_info->language);
    }
    state->own_dictionary_name = state->dictionary_name;
    apply_book_language(*state);
    state->token_view->set_hyphenation_language(state->dictionary_name);
    state->token_view->set_book_title(state->book_title);
    state->bookmarks_path = state->highlights_path;
    state->bookmarks_path.replace_extension(".bookmarks");
    state->bookmarks = load_bookmarks(state->bookmarks_path);
    update_token_view_title(seek_address);

    reader_set_work([this]() {
        return step_pagination();
    });

    // Update title info on scroll
    state->token_view->set_on_scroll([this](DocAddr address) {
        update_token_view_title(address);

        if (state->on_change_address)
        {
            // Lining up with a page can move the view again, so save where it ended up.
            state->on_change_address(state->token_view->get_address());
        }
    });
}

ReaderView::~ReaderView()
{
    try
    {
        if (kosync::load().auto_sync)
        {
            sync_push(*state, true);
        }
    }
    catch (...)
    {
    }
    library_log().flush();
    reader_clear_work();
    state->token_view_styling.unsubscribe_from_changes(
        state->token_view_styling_sub_id
    );
    book_layout::close();
}

void ReaderView::update_token_view_title(DocAddr address)
{
    const auto &toc = state->reader->get_table_of_contents();
    auto toc_position = state->reader->get_toc_position(address);

    if (toc_position.toc_index < toc.size())
    {
        state->token_view->set_title(toc[toc_position.toc_index].display_name);
    }
    else
    {
        state->token_view->set_title(state->filename);
    }

    bool chapter = false;
    if (state->token_view_styling.get_reading_mode() == ReadingMode::Page
        && state->token_view->align_to_page(chapter))
    {
        return;
    }

    const uint32_t chapter_percent = toc_position.progress_percent;
    const uint32_t book_percent = state->reader->get_global_progress_percent(address);
    const uint32_t progress_percent = chapter ? chapter_percent : book_percent;

    uint32_t page = 0;
    uint32_t pages = 0;
    if (state->token_view_styling.get_reading_mode() == ReadingMode::Page)
    {
        state->token_view->page_numbers(chapter, page, pages);
    }

    state->token_view->set_title_progress(
        progress_percent, page, pages, chapter_percent, book_percent);

    const auto book_extent = state->reader->get_progress_extent(address, false);

    // Chapter x of y, the pages left and the page of the book and of the chapter. All are exact:
    // the chapter's pages are read from the page count of the whole book.
    {
        int chapter_pages_left = -1;
        int book_pages_left = -1;
        int book_page = -1;
        int book_pages = -1;
        int chapter_page = -1;
        int chapter_pages = -1;
        if (pages > 0)
        {
            book_page = static_cast<int>(std::min(page, pages));
            book_pages = static_cast<int>(pages);
            book_pages_left = book_pages - book_page;
            uint32_t in_chapter = 0;
            uint32_t of_chapter = 0;
            if (state->token_view->chapter_page_numbers(in_chapter, of_chapter))
            {
                chapter_page = static_cast<int>(in_chapter);
                chapter_pages = static_cast<int>(of_chapter);
                chapter_pages_left = chapter_pages - chapter_page;
            }
        }
        const int index = toc_position.toc_index < toc.size() ? static_cast<int>(toc_position.toc_index) + 1 : 0;
        state->token_view->set_bar_extras(index, static_cast<int>(toc.size()), chapter_pages_left, book_pages_left);
        state->token_view->set_bar_pages(book_page, book_pages, chapter_page, chapter_pages);
    }

    // The printed page number, when the book lists its pages.
    std::string label;
    if (!(style_tweaks() & TWEAK_PLAIN_PAGES))
    {
        const auto &printed = state->reader->page_list();
        if (!printed.empty())
        {
            const auto after = std::upper_bound(printed.begin(), printed.end(), address, [](DocAddr value, const PageMark &mark) {
                return value < mark.address;
            });
            label = after == printed.begin() ? printed.front().label : (after - 1)->label;
        }
    }
    state->token_view->set_page_label(label);
    refresh_marks(*state);
}

bool ReaderView::counting_pages() const
{
    return state->token_view_styling.get_reading_mode() == ReadingMode::Page && !pagination_ready();
}

bool ReaderView::pagination_ready() const
{
    bool chapter = false;
    return state->token_view->page_index_ready(chapter);
}

bool ReaderView::run_pagination(uint32_t budget_ms)
{
    if (state->token_view_styling.get_reading_mode() != ReadingMode::Page)
    {
        return false;
    }

    bool chapter = false;
    if (state->token_view->page_index_ready(chapter))
    {
        return false;
    }

    // Count from the first line of the book or chapter in short slices between
    // frames, so a long count never stops the app from answering buttons.
    const Uint32 slice_start = SDL_GetTicks();
    while (state->token_view->advance_page_index(chapter))
    {
        if (SDL_GetTicks() - slice_start >= budget_ms)
        {
            return true;
        }
    }
    if (state->token_view->page_index_ready(chapter))
    {
        update_token_view_title(get_current_address(*state));
        return true;
    }
    return false;
}

bool ReaderView::step_pagination()
{
    return run_pagination(PAGINATION_SLICE_MS);
}

void ReaderView::settle_pagination(bool was_ready)
{
    if (!was_ready || state->token_view_styling.get_reading_mode() != ReadingMode::Page)
    {
        return;
    }
    // Only when the count was done before and this input made it start over, as
    // happens at the edge of a chapter. A first count of a whole book stays in
    // the background slices.
    if (!pagination_ready())
    {
        run_pagination(PAGINATION_SETTLE_MS);
    }
}

bool ReaderView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (counting_pages())
    {
        const bool chapter = false;
        const int percent = state->token_view->page_count_percent(chapter);
        if (!force_render && state->counting_shown && percent == state->counting_drawn)
        {
            return false;
        }
        state->counting_shown = true;
        state->counting_drawn = percent;

        progress_screen::draw(dest_surface, state->sys_styling, "Counting pages", "", percent);
        return true;
    }
    update_token_view_title(get_current_address(*state));
    // The text comes back after the count, so draw it whole once.
    const bool redraw = state->counting_shown;
    state->counting_shown = false;
    state->counting_drawn = -1;
    return state->token_view->render(dest_surface, force_render || redraw);
}

bool ReaderView::is_done()
{
    return state->is_done;
}

bool ReaderView::tick(uint32_t elapsed_ms)
{
    if (!state->sync_checked)
    {
        state->sync_checked = true;
        if (kosync::load().auto_sync)
        {
            sync_pull(*this, *state, true);
        }
    }
    return state->token_view->tick(elapsed_ms);
}

static void open_controls_guide(ReaderViewState &state)
{
    state.view_stack.push(std::make_shared<GuideView>(state.sys_styling));
}

// Puts the search at the front of the remembered searches.
static void remember_search(ReaderViewState &state, const std::string &query)
{
    std::string clean = query;
    for (char &c : clean)
    {
        if (c == '|')
        {
            c = ' ';
        }
    }
    std::vector<std::string> items = {clean};
    std::string current = state.token_view_styling.get_search_history();
    std::string piece;
    for (char c : current + "|")
    {
        if (c == '|')
        {
            if (!piece.empty() && piece != clean && items.size() < 8)
            {
                items.push_back(piece);
            }
            piece.clear();
        }
        else
        {
            piece.push_back(c);
        }
    }
    std::string joined;
    for (const std::string &item : items)
    {
        joined += (joined.empty() ? "" : "|") + item;
    }
    state.token_view_styling.set_search_history(joined);
}

// Marks the search words on the page and says which match the view is at.
static void show_match_note(ReaderViewState &state)
{
    if (!state.search_hits || state.search_hits->empty())
    {
        state.token_view->set_search_marks("", "");
        return;
    }
    state.token_view->set_search_marks(
        state.search_query,
        "Match " + std::to_string(state.search_index + 1) + " / " + std::to_string(state.search_hits->size())
    );
}

// Goes to the next or the previous match of the last search, round the ends.
static void step_match(ReaderView &reader_view, ReaderViewState &state, int direction)
{
    if (!state.search_hits || state.search_hits->empty())
    {
        return;
    }
    const size_t count = state.search_hits->size();
    state.search_index = (state.search_index + (direction > 0 ? 1 : count - 1)) % count;
    show_match_note(state);
    state.token_view->cancel_word_select();
    reader_view.jump_to((*state.search_hits)[state.search_index].address);
}

// Finds the word or phrase everywhere in the book and lists the places, with the chapter of each.
// The scan reads the whole book, so a bar shows how far it has got.
static void run_search(ReaderView &reader_view, ReaderViewState &state, const std::string &word)
{
    SystemStyling &styling = state.sys_styling;
    constexpr size_t MAX_HITS = 300;
    SearchOptions options;
    options.whole_word = (state.token_view_styling.get_search_options() & 1) != 0;
    options.match_case = (state.token_view_styling.get_search_options() & 2) != 0;
    options.match_accents = (state.token_view_styling.get_search_options() & 4) != 0;
    if ((state.token_view_styling.get_search_options() & 8) != 0)
    {
        // This chapter only: from the start of the chapter to the start of the next one.
        const auto &toc = state.reader->get_table_of_contents();
        if (!toc.empty())
        {
            const uint32_t index = state.reader->get_toc_position(get_current_address(state)).toc_index;
            options.from_address = state.reader->get_toc_item_address(index);
            for (uint32_t next = index + 1; next < toc.size(); ++next)
            {
                const DocAddr address = state.reader->get_toc_item_address(next);
                if (address > options.from_address)
                {
                    options.until_address = address;
                    break;
                }
            }
        }
    }
    remember_search(state, word);
    const auto hits = std::make_shared<std::vector<SearchHit>>(search_book(
        *state.reader,
        word,
        MAX_HITS,
        [&styling, &word](int percent) {
            progress_screen::show(styling, "Searching", word, percent);
        },
        options
    ));
    // The search screen was drawn straight to the display, so draw the page again.
    state.token_view->invalidate();

    if (hits->empty())
    {
        show_message(state, "No match for \"" + word + "\"");
        return;
    }

    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    std::vector<std::string> names;
    std::vector<std::string> details;
    const auto &toc = state.reader->get_table_of_contents();
    for (const SearchHit &hit : *hits)
    {
        names.push_back(hit.snippet);
        std::string detail = percent_label(state, hit.address);
        if (!toc.empty())
        {
            const uint32_t chapter = state.reader->get_toc_position(hit.address).toc_index;
            if (chapter < toc.size())
            {
                detail += "  " + toc[chapter].display_name;
            }
        }
        details.push_back(detail);
    }
    menu->set_entries(names);
    menu->set_details(details);
    menu->set_title(
        "Search: " + word + " (" + std::to_string(hits->size()) + (hits->size() >= MAX_HITS ? "+" : "") + ")"
    );
    menu->set_hints({"A  Go", "B  Close"});
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, hits, word](uint32_t index) {
        if (index < hits->size())
        {
            state.token_view->cancel_word_select();
            // From now on the matches can be stepped through without the list.
            state.search_hits = hits;
            state.search_query = word;
            state.search_index = index;
            show_match_note(state);
            reader_view.jump_to((*hits)[index].address);
        }
    });
    state.view_stack.push(menu);
}

// The search menu of the book menu: a new search, the two options, and the last searches.
void open_search_menu(ReaderView &reader_view, ReaderViewState &state)
{
    const int bits = state.token_view_styling.get_search_options();
    std::vector<std::string> history;
    {
        std::string piece;
        for (char c : state.token_view_styling.get_search_history() + "|")
        {
            if (c == '|')
            {
                if (!piece.empty())
                {
                    history.push_back(piece);
                }
                piece.clear();
            }
            else
            {
                piece.push_back(c);
            }
        }
    }
    std::vector<std::string> names = {
        "Type a search...",
        std::string("Whole words: ") + ((bits & 1) ? "On" : "Off"),
        std::string("Match case: ") + ((bits & 2) ? "On" : "Off"),
        std::string("Match accents: ") + ((bits & 4) ? "On" : "Off"),
        std::string("This chapter only: ") + ((bits & 8) ? "On" : "Off")
    };
    std::vector<std::string> details = {"", "", "", "", ""};
    for (const std::string &item : history)
    {
        names.push_back(item);
        details.push_back("recent");
    }
    auto menu = std::make_shared<SelectionMenu>(names, state.sys_styling);
    menu->set_details(details);
    menu->set_title("Search in book");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, history, bits](uint32_t index) {
        if (index == 0)
        {
            state.view_stack.push(std::make_shared<TextInputView>(
                "Search in book",
                history.empty() ? std::string() : history.front(),
                [&reader_view, &state](const std::string &typed) {
                    std::string query = typed;
                    while (!query.empty() && query.back() == ' ') query.pop_back();
                    while (!query.empty() && query.front() == ' ') query.erase(0, 1);
                    if (!query.empty())
                    {
                        run_search(reader_view, state, query);
                    }
                },
                state.sys_styling,
                60
            ));
        }
        else if (index >= 1 && index <= 4)
        {
            static const int FLAGS[] = {0, 1, 2, 4, 8};
            state.token_view_styling.set_search_options(bits ^ FLAGS[index]);
            open_search_menu(reader_view, state);
        }
        else if (index - 5 < history.size())
        {
            run_search(reader_view, state, history[index - 5]);
        }
    });
    state.view_stack.push(menu);
}
// The entry for the base form of an inflected word, when the dictionary has one: the word it is and the
// text. For a lookup of "running" that has no entry, "run" may.
std::optional<std::pair<std::string, std::string>> base_form_entry(Dictionary &dictionary, const std::string &word, const std::string &dictionary_name)
{
    if (word.empty())
    {
        return std::nullopt;
    }
    for (const std::string &candidate : word_forms::base_forms(word, word_forms::language_for(dictionary_name)))
    {
        if (auto gloss = dictionary.lookup(candidate))
        {
            return std::make_pair(candidate, *gloss);
        }
    }
    return std::nullopt;
}

// "In this book: 14 times" for a single word, with "(rare)" when it turns up only once to three times,
// so the words worth keeping stand out. Empty for a phrase, or when the style tweak turns it off.
std::string word_count_note(ReaderViewState &state, const std::string &lookup)
{
    if ((state.token_view_styling.get_tweaks() & TWEAK_NO_WORD_COUNT) || lookup.empty() || lookup.find(' ') != std::string::npos || !state.reader)
    {
        return "";
    }
    constexpr size_t LIMIT = 500;
    std::vector<SearchHit> hits;
    try
    {
        hits = search_book(*state.reader, lookup, LIMIT, [](int) {}, SearchOptions());
    }
    catch (...)
    {
        return "";
    }
    if (hits.empty())
    {
        return "";
    }
    const size_t count = hits.size();
    std::string note = "In this book: ";
    if (count >= LIMIT)
    {
        note += std::to_string(LIMIT) + "+ times";
    }
    else if (count == 1)
    {
        note += "once";
    }
    else
    {
        note += std::to_string(count) + " times";
    }
    if (count <= 3)
    {
        note += " (rare)";
    }
    return note;
}

// Looks the text up in the dictionary of the book language, falling back to English, and shows it.
// A phrase that has no entry of its own is looked up word by word.
void show_definition(ReaderViewState &state, const std::string &lookup, const std::string &chosen)
{
    std::map<std::string, Dictionary> &dictionaries = dictionary_cache();
    const std::string sentence = state.lookup_sentence;
    state.lookup_sentence.clear();
    // The dictionary asked for now, else the one in Settings, else the one for the language of the book.
    const std::string dictionary_name = !chosen.empty() ? chosen
        : (!state.token_view_styling.get_dictionary().empty() ? state.token_view_styling.get_dictionary() : state.dictionary_name);
    SystemStyling &styling = state.sys_styling;
    const auto load_with_bar = [&styling](Dictionary &d, const std::string &label) {
        if (!d.is_loaded())
        {
            d.load([&styling, &label](int percent) {
                progress_screen::show(styling, "Loading dictionary", label, percent);
            });
        }
    };
    if (dictionary_name == ALL_DICTIONARIES && !lookup.empty())
    {
        // Every dictionary that knows the word, one section each.
        const long long when = static_cast<long long>(std::time(nullptr));
        std::string all_body;
        int sections = 0;
        for (const std::string &name : dictionary_names())
        {
            Dictionary &each = dictionaries.try_emplace(name, name).first->second;
            load_with_bar(each, name);
            if (!each.available())
            {
                continue;
            }
            if (auto gloss = each.lookup(lookup))
            {
                all_body += (all_body.empty() ? "" : "\n\n") + std::string("[") + name + "]\n" + *gloss;
                if (sections == 0)
                {
                    vocabulary::add(lookup, *gloss, state.book_title, when, sentence);
                }
                ++sections;
            }
        }
        if (sections == 0)
        {
            // None has it: the dictionary of the book may still offer near words.
            show_definition(state, lookup, state.dictionary_name);
            return;
        }
        {
            const std::string note = word_count_note(state, lookup);
            if (!note.empty())
            {
                all_body = note + "\n\n" + all_body;
            }
        }
        auto entry = std::make_shared<DefinitionView>(lookup, all_body, state.sys_styling);
        entry->set_lookup([&state, chosen](const std::string &word) { show_definition(state, word, chosen); });
        state.view_stack.push(entry);
        return;
    }
    Dictionary *dictionary = &dictionaries.try_emplace(dictionary_name, dictionary_name).first->second;
    load_with_bar(*dictionary, dictionary_name);
    if (!dictionary->available() && dictionary_name != "english")
    {
        // The book's language has no dictionary on the card.
        dictionary = &dictionaries.try_emplace("english", "english").first->second;
        load_with_bar(*dictionary, "english");
    }
    const long long now = static_cast<long long>(std::time(nullptr));
    std::string body;
    if (lookup.empty())
    {
        body = "Not a word";
    }
    else if (!dictionary->available())
    {
        body = "No dictionary installed";
    }
    else if (auto gloss = dictionary->lookup(lookup))
    {
        body = *gloss;
        vocabulary::add(lookup, *gloss, state.book_title, now, sentence);
        const std::string note = word_count_note(state, lookup);
        if (!note.empty())
        {
            body = note + "\n\n" + body;
        }
    }
    else if (const auto base = lookup.find(' ') == std::string::npos ? base_form_entry(*dictionary, lookup, dictionary_name) : std::nullopt)
    {
        // "running" has no entry but "run" has: show that, and say so.
        body = "(" + base->first + ")\n\n" + base->second;
        vocabulary::add(base->first, base->second, state.book_title, now, sentence);
        const std::string note = word_count_note(state, lookup);
        if (!note.empty())
        {
            body = note + "\n\n" + body;
        }
    }
    else
    {
        // Several words with no entry as a whole: give each word its own entry.
        std::vector<std::string> words;
        {
            std::string word;
            for (char c : lookup + " ")
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
        if (words.size() > 1)
        {
            int shown = 0;
            for (const std::string &word : words)
            {
                if (shown >= 6)
                {
                    break;
                }
                if (auto part = dictionary->lookup(word))
                {
                    body += (body.empty() ? "" : "\n\n") + word + "\n" + *part;
                    vocabulary::add(word, *part, state.book_title, now, sentence);
                    ++shown;
                }
            }
        }
        if (body.empty())
        {
            // Offer the nearest words instead of a dead end.
            const std::vector<std::string> near = words.size() == 1 ? dictionary->suggest(lookup) : std::vector<std::string>();
            if (!near.empty())
            {
                auto pick = std::make_shared<SelectionMenu>(near, state.sys_styling);
                pick->set_title("No entry: " + lookup);
                pick->set_details(std::vector<std::string>(near.size(), ""));
                pick->set_compact(true);
                pick->set_close_on_select();
                pick->set_hints({"A  Look up", "B  Close"});
                pick->set_on_selection([&state, near, chosen](uint32_t index) {
                    if (index < near.size())
                    {
                        show_definition(state, near[index], chosen);
                    }
                });
                state.view_stack.push(pick);
                return;
            }
            body = "No entry for \"" + lookup + "\"";
        }
    }
    auto entry = std::make_shared<DefinitionView>(
        lookup.empty() ? std::string("Look up") : lookup,
        body,
        state.sys_styling
    );
    if (!lookup.empty() && dictionary->available())
    {
        // A word of the entry can be looked up in turn, in the same dictionary.
        entry->set_lookup([&state, chosen](const std::string &word) { show_definition(state, word, chosen); });
    }
    state.view_stack.push(entry);
}

static void open_dictionary_browser(ReaderViewState &state)
{
    // The dictionary of the settings, or of the book when the settings ask for all of them.
    std::string name = !state.token_view_styling.get_dictionary().empty() ? state.token_view_styling.get_dictionary() : state.dictionary_name;
    if (name == ALL_DICTIONARIES)
    {
        name = state.dictionary_name;
    }
    SystemStyling &styling = state.sys_styling;
    Dictionary *dictionary = &dictionary_cache().try_emplace(name, name).first->second;
    const auto load = [&styling](Dictionary &d, const std::string &label) {
        if (!d.is_loaded())
        {
            d.load([&styling, &label](int percent) {
                progress_screen::show(styling, "Loading dictionary", label, percent);
            });
        }
    };
    load(*dictionary, name);
    if (!dictionary->available() && name != "english")
    {
        name = "english";
        dictionary = &dictionary_cache().try_emplace(name, name).first->second;
        load(*dictionary, name);
    }
    if (!dictionary->available())
    {
        auto note = std::make_shared<DefinitionView>("Dictionary", "No dictionary installed", state.sys_styling);
        state.view_stack.push(note);
        return;
    }
    state.view_stack.push(std::make_shared<TextInputView>(
        "Words starting with (empty for the start)",
        "",
        [&state, dictionary, name](const std::string &typed) {
            std::string prefix = lookup_lowercase(typed);
            while (!prefix.empty() && prefix.back() == ' ') prefix.pop_back();
            while (!prefix.empty() && prefix.front() == ' ') prefix.erase(0, 1);
            const std::vector<std::string> words = dictionary->words_with_prefix(prefix);
            if (words.empty())
            {
                state.view_stack.push(std::make_shared<DefinitionView>("Dictionary", "No words start with \"" + prefix + "\"", state.sys_styling));
                return;
            }
            auto list = std::make_shared<SelectionMenu>(words, state.sys_styling);
            list->set_title(name + ": " + (prefix.empty() ? std::string("all words") : prefix + "..."));
            list->set_compact(true);
            list->set_hints({"A  Open", "B  Back"});
            list->set_on_selection([&state, words, name](uint32_t index) {
                if (index < words.size())
                {
                    show_definition(state, words[index], name);
                }
            });
            state.view_stack.push(list);
        },
        state.sys_styling,
        40
    ));
}

static void open_word_actions(ReaderView &reader_view, ReaderViewState &state)
{
    std::string lookup = state.token_view->selected_lookup();
    const std::string sentence = state.token_view->selected_sentence();
    bool marked = state.token_view->selected_is_highlighted();
    const auto link = state.token_view->selected_link();

    // What the menu offers, in order. The footnote and link rows only appear on a link.
    enum class Act { Note, GoTo, LookUp, LookUpIn, Search, Highlight, Underline, Strikeout };
    std::vector<Act> acts;
    std::vector<std::string> entries;
    std::optional<LinkTarget> target;
    if (link)
    {
        target = state.reader->resolve_link(*link);
        if (target && target->has_fragment && !target->text.empty())
        {
            acts.push_back(Act::Note);
            entries.push_back("Show note");
        }
        if (target)
        {
            acts.push_back(Act::GoTo);
            entries.push_back("Go to link");
        }
    }
    acts.push_back(Act::LookUp);
    entries.push_back("Look up");
    if (!lookup.empty() && dictionary_names().size() > 1)
    {
        acts.push_back(Act::LookUpIn);
        entries.push_back("Look up in...");
    }
    if (!lookup.empty())
    {
        acts.push_back(Act::Search);
        entries.push_back("Search book");
    }
    acts.push_back(Act::Highlight);
    entries.push_back(marked ? "Remove highlight" : "Highlight");
    if (!marked)
    {
        acts.push_back(Act::Underline);
        entries.push_back("Underline");
        acts.push_back(Act::Strikeout);
        entries.push_back("Strike out");
    }

    auto menu = std::make_shared<SelectionMenu>(entries, state.sys_styling);
    menu->set_title(lookup.empty() ? std::string("Word") : lookup);
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, lookup, sentence, acts, target, dictionary_name = state.dictionary_name](uint32_t index) {
        if (index >= acts.size())
        {
            return;
        }
        switch (acts[index])
        {
            case Act::Note:
                state.view_stack.push(std::make_shared<DefinitionView>(
                    "Note",
                    target->text,
                    state.sys_styling
                ));
                break;
            case Act::GoTo:
                state.token_view->cancel_word_select();
                reader_view.jump_to(target->address);
                break;
            case Act::LookUp:
                state.lookup_sentence = sentence;
                show_definition(state, lookup);
                break;
            case Act::LookUpIn:
            {
                std::vector<std::string> names = dictionary_names();
                names.insert(names.begin(), ALL_DICTIONARIES);
                std::vector<std::string> shown = names;
                shown[0] = "All dictionaries";
                auto pick = std::make_shared<SelectionMenu>(shown, state.sys_styling);
                pick->set_title("Look up in");
                pick->set_compact(true);
                pick->set_close_on_select();
                pick->set_on_selection([&state, names, lookup, sentence](uint32_t chosen) {
                    if (chosen < names.size())
                    {
                        state.lookup_sentence = sentence;
                        show_definition(state, lookup, names[chosen]);
                    }
                });
                state.view_stack.push(pick);
                break;
            }
            case Act::Highlight:
                toggle_highlight_with_undo(state, marker_style(state.token_view_styling.get_highlight_color()));
                break;
            case Act::Search:
                run_search(reader_view, state, lookup);
                break;
            case Act::Underline:
                toggle_highlight_with_undo(state, HighlightStyle::Underline);
                break;
            case Act::Strikeout:
                toggle_highlight_with_undo(state, HighlightStyle::Strikeout);
                break;
        }
    });
    state.view_stack.push(menu);
}
// Pressing forward on the last page: offer the book that comes next.
static void offer_next_book(ReaderViewState &state)
{
    const auto next = next_book::find_next(state.book_path, next_book::shared_cache(), false);
    std::string next_title;
    if (next)
    {
        next_book::shared_cache().load(next->path);
        const BookInfo *info = next_book::shared_cache().get(next->path);
        next_title = std::string(next->by_series ? "Next in series: " : "Next in folder: ")
            + (info && !info->title.empty() ? info->title : next->path.stem().string());
    }
    const auto summary = reading_stats::summarize(state.book_path, static_cast<long long>(std::time(nullptr)));
    const LibraryLog::Entry *log = library_log().get(state.book_path);

    enum class Row { Next, Rating, Info, Back, Stay };
    auto rows = std::make_shared<std::vector<Row>>();
    auto menu = std::make_shared<SelectionMenu>(std::vector<std::string> {}, state.sys_styling);
    std::weak_ptr<SelectionMenu> weak = menu;
    // The rows are drawn again when the rating changes.
    auto fill = [&state, next, next_title, summary, rows](SelectionMenu &m) {
        rows->clear();
        std::vector<std::string> names;
        std::vector<std::string> details;
        const auto add = [&](Row kind, const std::string &name, const std::string &detail) {
            rows->push_back(kind);
            names.push_back(name);
            details.push_back(detail);
        };
        if (next)
        {
            add(Row::Next, next_title, "");
        }
        const LibraryLog::Entry *now = library_log().get(state.book_path);
        const int stars = now ? now->rating : 0;
        add(Row::Rating, "Your rating (A changes it)", stars > 0 ? std::string(static_cast<size_t>(stars), '*') + std::string(static_cast<size_t>(5 - stars), '-') : std::string("not rated"));
        add(Row::Info, "Time spent reading it", reading_stats::format_duration(summary.book_total));
        add(Row::Info, "Books finished so far", std::to_string(library_log().finished_count()));
        add(Row::Back, "Back to the list", "");
        add(Row::Stay, "Stay here", "");
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_title("You finished " + state.book_title);
    menu->set_compact(true);
    menu->set_hints({"A  Choose", "B  Stay"});
    menu->set_on_selection([&state, next, rows, weak, fill](uint32_t index) {
        auto open = weak.lock();
        if (!open || index >= rows->size())
        {
            return;
        }
        switch ((*rows)[index])
        {
            case Row::Next:
                next_book::request(next->path);
                state.is_done = true;
                open->close();
                break;
            case Row::Rating:
            {
                const LibraryLog::Entry *now = library_log().get(state.book_path);
                library_log().set_rating(state.book_path, ((now ? now->rating : 0) + 1) % 6);
                library_log().flush();
                fill(*open);
                open->set_cursor_pos(index);
                break;
            }
            case Row::Back:
                state.is_done = true;
                open->close();
                break;
            case Row::Stay:
                open->close();
                break;
            case Row::Info:
                break;
        }
    });
    state.view_stack.push(menu);
}

void ReaderView::on_keypress(SDLKey key)
{
    reading_stats::note_activity(state->book_path, static_cast<long long>(std::time(nullptr)));
    if (counting_pages())
    {
        // Nothing to read until the count is done. B still leaves the book.
        if (key == SW_BTN_B)
        {
            state->is_done = true;
        }
        return;
    }
    const bool was_ready = pagination_ready();
    handle_keypress(key);
    settle_pagination(was_ready);
}

void ReaderView::run_button_action(int action)
{
    switch (action)
    {
        case button_actions::ACTION_BOOKMARK:
            toggle_bookmark(*state);
            break;
        case button_actions::ACTION_TITLE_BAR:
            state->token_view_styling.set_show_title_bar(!state->token_view_styling.get_show_title_bar());
            break;
        case button_actions::ACTION_CHAPTERS:
            if (!state->reader->get_table_of_contents().empty())
            {
                open_toc_menu(*this, *state);
            }
            break;
        case button_actions::ACTION_SEARCH:
            open_search_menu(*this, *state);
            break;
        case button_actions::ACTION_GO_TO_PERCENT:
        {
            const uint32_t now = state->reader->get_global_progress_percent(get_current_address(*state));
            state->view_stack.push(std::make_shared<GotoView>(
                "Go to", "%", 0, 100, now,
                [this](uint32_t percent) { jump_to(state->reader->get_address_at_percent(percent)); },
                state->sys_styling
            ));
            break;
        }
        case button_actions::ACTION_BOOK_MAP:
        {
            std::vector<DocAddr> bookmark_addresses;
            for (const Bookmark &bookmark : state->bookmarks)
            {
                bookmark_addresses.push_back(bookmark.address);
            }
            std::vector<DocAddr> highlight_addresses;
            for (const Highlight &highlight : state->token_view->highlights())
            {
                highlight_addresses.push_back(highlight.first);
            }
            state->view_stack.push(std::make_shared<BookMapView>(
                state->reader,
                get_current_address(*state),
                bookmark_addresses,
                highlight_addresses,
                [this](DocAddr address) { jump_to(address); },
                state->sys_styling
            ));
            break;
        }
        case button_actions::ACTION_JUMP_BACK:
            jump_back();
            break;
        case button_actions::ACTION_BOOK_MENU:
            open_book_menu();
            break;
        case button_actions::ACTION_PICK_WORD:
            state->token_view->start_word_select();
            break;
        case button_actions::ACTION_NEXT_PROFILE:
        {
            static size_t profile_index = 0;
            const std::vector<std::string> names = layout_profiles::names();
            if (names.empty())
            {
                show_message(*state, "No layout profiles saved yet");
                break;
            }
            profile_index = (profile_index + 1) % names.size();
            layout_profiles::apply(names[profile_index], state->sys_styling, state->token_view_styling);
            show_message(*state, "Layout: " + names[profile_index]);
            break;
        }
        case button_actions::ACTION_NEXT_THEME:
            state->sys_styling.set_color_theme(get_next_theme(state->sys_styling.get_color_theme()));
            break;
        case button_actions::ACTION_BIGGER_TEXT:
            state->sys_styling.set_font_size(state->sys_styling.get_next_font_size());
            break;
        case button_actions::ACTION_SMALLER_TEXT:
            state->sys_styling.set_font_size(state->sys_styling.get_prev_font_size());
            break;
        case button_actions::ACTION_LOOK_UP:
            state->view_stack.push(std::make_shared<TextInputView>(
                "Look up a word",
                "",
                [this](const std::string &typed) {
                    std::string word = lookup_lowercase(typed);
                    while (!word.empty() && word.back() == ' ') word.pop_back();
                    while (!word.empty() && word.front() == ' ') word.erase(0, 1);
                    if (!word.empty())
                    {
                        show_definition(*state, word);
                    }
                },
                state->sys_styling,
                60
            ));
            break;
        case button_actions::ACTION_WORD_LIST:
            open_word_list(*state);
            break;
        case button_actions::ACTION_HIGHLIGHTS:
            open_highlights_menu(*this, *state);
            break;
        case button_actions::ACTION_BOOKMARKS:
            open_bookmarks_menu(*this, *state);
            break;
        case button_actions::ACTION_NEXT_CHAPTER:
            jump_chapter(*this, *state, 1);
            break;
        case button_actions::ACTION_PREVIOUS_CHAPTER:
            jump_chapter(*this, *state, -1);
            break;
        default:
            break;
    }
}

void ReaderView::handle_keypress(SDLKey key)
{
    if (state->token_view->is_word_selecting())
    {
        if (key == SW_BTN_B || key == SW_BTN_Y)
        {
            state->token_view->cancel_word_select();
            return;
        }
        if (key == SW_BTN_A)
        {
            open_word_actions(*this, *state);
            return;
        }
        if (key == SW_BTN_START)
        {
            state->token_view->toggle_extend_selection();
            return;
        }
        if (key == SW_BTN_X)
        {
            state->token_view->grow_selection();
            return;
        }
        state->token_view->on_keypress(key);
        return;
    }

    if (!state->replaying_tap && (key == SW_BTN_START || key == SW_BTN_A || key == SW_BTN_Y))
    {
        const auto hold_slot = key == SW_BTN_START ? button_actions::SLOT_HOLD_START
            : (key == SW_BTN_A ? button_actions::SLOT_HOLD_A : button_actions::SLOT_HOLD_Y);
        if (button_actions::get(hold_slot) != button_actions::ACTION_NOTHING)
        {
            state->pending_key = key;
            state->pending_fired = false;
            return;
        }
    }

    if (key == SW_BTN_START)
    {
        run_button_action(button_actions::get(button_actions::SLOT_START));
        return;
    }

    if (key == SW_BTN_Y)
    {
        state->token_view->start_word_select();
        return;
    }

    if (key == SW_BTN_B)
    {
        state->is_done = true;
        return;
    }

    if (!state->token_view->is_word_selecting() && state->token_view_styling.get_ruler() == 0 && state->token_view->at_end())
    {
        const auto shoulders = get_shoulder_keymap_lr(state->sys_styling.get_shoulder_keymap());
        if (key == SW_BTN_DOWN || key == SW_BTN_RIGHT || key == shoulders.second)
        {
            offer_next_book(*state);
            return;
        }
    }

    switch (key) {
        case SW_BTN_A:
            run_button_action(button_actions::get(button_actions::SLOT_A));
            break;
        case SW_BTN_SELECT:
            open_book_menu();
            break;
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
        {
            const auto pages = get_shoulder_keymap_lr(state->sys_styling.get_shoulder_keymap());
            if (key == pages.first || key == pages.second)
            {
                state->token_view->on_keypress(key);
            }
            else if (state->search_hits && !state->search_hits->empty())
            {
                // While a search is open the pair steps between its matches.
                step_match(*this, *state, (key == SW_BTN_L1 || key == SW_BTN_L2) ? -1 : 1);
            }
            else
            {
                jump_chapter(*this, *state, (key == SW_BTN_L1 || key == SW_BTN_L2) ? -1 : 1);
            }
            break;
        }
        default:
            state->token_view->on_keypress(key);
            break;
    }
}

void ReaderView::on_keyrelease(SDLKey key)
{
    if (key != state->pending_key || key == SDLK_UNKNOWN)
    {
        return;
    }
    const bool fired = state->pending_fired;
    state->pending_key = SDLK_UNKNOWN;
    state->pending_fired = false;
    if (!fired && !counting_pages())
    {
        // It was a tap: do what the key does when pressed.
        state->replaying_tap = true;
        const bool was_ready = pagination_ready();
        handle_keypress(key);
        settle_pagination(was_ready);
        state->replaying_tap = false;
    }
}

void ReaderView::on_keyheld(SDLKey key, uint32_t hold_time_ms)
{
    if (counting_pages())
    {
        return;
    }
    if (key == state->pending_key && !state->pending_fired && hold_time_ms >= button_actions::HOLD_MILLISECONDS)
    {
        state->pending_fired = true;
        const auto hold_slot = key == SW_BTN_START ? button_actions::SLOT_HOLD_START
            : (key == SW_BTN_A ? button_actions::SLOT_HOLD_A : button_actions::SLOT_HOLD_Y);
        const bool was_ready = pagination_ready();
        run_button_action(button_actions::get(hold_slot));
        settle_pagination(was_ready);
        return;
    }
    state->token_view->on_keyheld(key, hold_time_ms);
}

void ReaderView::set_on_change_address(std::function<void(DocAddr)> callback)
{
    state->on_change_address = callback;
}

void ReaderView::seek_to_toc_index(uint32_t toc_index)
{
    jump_to(state->reader->get_toc_item_address(toc_index));
}

void ReaderView::jump_to(DocAddr address)
{
    const DocAddr here = get_current_address(*state);
    if (address == here)
    {
        return;
    }
    if (state->jump_stack.empty() || state->jump_stack.back() != here)
    {
        state->jump_stack.push_back(here);
        if (state->jump_stack.size() > 20)
        {
            state->jump_stack.erase(state->jump_stack.begin());
        }
    }
    seek_to_address(address);
}

void ReaderView::go_to_page(bool chapter, uint32_t page)
{
    const DocAddr here = get_current_address(*state);
    if (state->jump_stack.empty() || state->jump_stack.back() != here)
    {
        state->jump_stack.push_back(here);
        if (state->jump_stack.size() > 20)
        {
            state->jump_stack.erase(state->jump_stack.begin());
        }
    }
    state->token_view->go_to_page(chapter, page);
}

bool ReaderView::can_jump_back() const
{
    return !state->jump_stack.empty();
}

bool ReaderView::jump_back()
{
    if (state->jump_stack.empty())
    {
        return false;
    }
    const DocAddr address = state->jump_stack.back();
    state->jump_stack.pop_back();
    seek_to_address(address);
    return true;
}

void ReaderView::seek_to_address(DocAddr address)
{
    if (state->token_view)
    {
        const bool was_ready = pagination_ready();
        state->token_view->seek_to_address(address);
        settle_pagination(was_ready);
        update_token_view_title(address);

        if (state->on_change_address)
        {
            state->on_change_address(address);
        }
    }
}
