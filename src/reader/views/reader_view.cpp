#include "./reader_view.h"

#include "./definition_view.h"
#include "./goto_view.h"
#include "./book_map_view.h"
#include "./image_view.h"
#include "./page_browser_view.h"
#include "./text_input_view.h"
#include "./popup_view.h"
#include "./selection_menu.h"
#include "./menu_draw.h"
#include "reader/progress_screen.h"
#include "reader/book_info.h"
#include "reader/book_layout.h"
#include "reader/bookmarks.h"
#include "reader/controls_guide.h"
#include "./token_view/token_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/dictionary.h"
#include "reader/text_style.h"
#include "reader/library_log.h"
#include "reader/reading_speed.h"
#include "reader/book_search.h"
#include "reader/reading_stats.h"
#include "reader/vocabulary.h"
#include "reader/reading_mode.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "reader/work_pump.h"
#include "reader/kosync.h"
#include "reader/view_stack.h"
#include "reader/wifi_upload.h"
#include "./reading_history_view.h"

#include "doc_api/doc_reader.h"
#include <ctime>
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <SDL/SDL.h>

#include <algorithm>
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

struct ReaderViewState
{
    bool is_done = false;

    std::function<void(DocAddr)> on_change_address;

    std::string filename;
    std::shared_ptr<DocReader> reader;
    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t token_view_styling_sub_id;

    ViewStack &view_stack;

    std::filesystem::path book_path;
    std::string book_title;
    std::string dictionary_name = "english"; // follows the language of the book
    std::filesystem::path highlights_path;
    std::filesystem::path bookmarks_path;
    std::vector<Bookmark> bookmarks;
    std::vector<DocAddr> jump_stack;
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
void open_toc_menu(ReaderView &reader_view, ReaderViewState &state, bool folded = false)
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
    folded = folded && has_nested;

    // The rows shown, as indexes into the table of contents.
    auto visible = std::make_shared<std::vector<uint32_t>>();
    std::vector<std::string> menu_names;
    std::vector<std::string> menu_details;
    size_t marker_row = 0;
    for (uint32_t i = 0; i < toc.size(); ++i)
    {
        if (folded && toc[i].indent_level > 0)
        {
            continue;
        }
        if (i <= current_toc_index)
        {
            marker_row = visible->size();
        }
        visible->push_back(i);
        menu_names.push_back(std::string(toc[i].indent_level * 2, ' ') + toc[i].display_name);

        const DocAddr start = state.reader->get_toc_item_address(i);
        const uint32_t length = state.reader->get_progress_extent(start, true).second;
        std::string detail;
        if (i < current_toc_index)
        {
            detail = "Read";
        }
        else
        {
            uint32_t left = length;
            std::string percent;
            if (i == current_toc_index)
            {
                const uint32_t done = state.reader->get_toc_position(get_current_address(state)).progress_percent;
                percent = std::to_string(done) + "%  ";
                left = static_cast<uint32_t>(static_cast<unsigned long long>(length) * (100 - std::min<uint32_t>(done, 100)) / 100);
            }
            const long long seconds = reading_speed::seconds_for(left);
            detail = percent + (seconds >= 0 ? reading_stats::format_duration(seconds) : std::string());
        }
        menu_details.push_back(detail);
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
    toc_select_menu->set_title(folded ? "Chapters (folded)" : "Chapters");
    if (has_nested)
    {
        toc_select_menu->set_uses_x(true);
        toc_select_menu->set_hints({"A  Go", folded ? "X  Unfold" : "X  Fold", "B  Close"});
    }

    // select current toc item
    toc_select_menu->set_marker(static_cast<uint32_t>(marker_row));
    toc_select_menu->set_cursor_pos(static_cast<uint32_t>(marker_row));

    toc_select_menu->set_default_on_keypress([&reader_view, &state, folded, has_nested](SDLKey key, SelectionMenu &toc) {
        if (key == SW_BTN_SELECT)
        {
            toc.close();
        }
        else if (key == SW_BTN_X && has_nested)
        {
            toc.close();
            open_toc_menu(reader_view, state, !folded);
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
    show_message(state, "Bookmark added");
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
    menu->set_hints({"A  Go", "X  Delete", "B  Close"});
    menu->set_uses_x(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state](uint32_t index) {
        if (index < state.bookmarks.size())
        {
            reader_view.jump_to(state.bookmarks[index].address);
        }
    });
    menu->set_default_on_keypress([&state, fill](SDLKey key, SelectionMenu &m) {
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= state.bookmarks.size())
        {
            return;
        }
        state.bookmarks.erase(state.bookmarks.begin() + index);
        save_bookmarks(state.bookmarks_path, state.bookmarks);
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

void open_highlights_menu(ReaderView &reader_view, ReaderViewState &state)
{
    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    auto order = std::make_shared<std::vector<size_t>>();
    auto fill = [&state, order](SelectionMenu &m) {
        const auto &highlights = state.token_view->highlights();
        *order = highlight_order(highlights);
        std::vector<std::string> names;
        std::vector<std::string> details;
        for (size_t index : *order)
        {
            const Highlight &highlight = highlights[index];
            names.push_back(highlight.text.empty() ? std::string("(text not saved)") : highlight.text);
            details.push_back(percent_label(state, highlight.first));
        }
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_title("Highlights");
    menu->set_empty_message("No highlights yet\nPress Y, pick words, then A");
    menu->set_hints({"A  Go", "X  Delete", "B  Close"});
    menu->set_uses_x(true);
    menu->set_close_on_select();
    menu->set_on_selection([&reader_view, &state, order](uint32_t index) {
        if (index < order->size())
        {
            reader_view.jump_to(state.token_view->highlights()[(*order)[index]].first);
        }
    });
    menu->set_default_on_keypress([&state, order, fill](SDLKey key, SelectionMenu &m) {
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= order->size())
        {
            return;
        }
        state.token_view->remove_highlight((*order)[index]);
        fill(m);
        m.set_cursor_pos(std::min<uint32_t>(index, order->empty() ? 0 : static_cast<uint32_t>(order->size()) - 1));
    });
    state.view_stack.push(menu);
}

void open_word_list(ReaderViewState &state)
{
    auto menu = std::make_shared<SelectionMenu>(state.sys_styling);
    auto fill = [](SelectionMenu &m) {
        std::vector<std::string> names;
        std::vector<std::string> details;
        for (const vocabulary::Entry &entry : vocabulary::entries())
        {
            names.push_back(entry.word);
            details.push_back(entry.book);
        }
        m.set_entries(names);
        m.set_details(details);
    };
    fill(*menu);
    menu->set_title("Word list");
    menu->set_empty_message("No words yet\nLook a word up and it is kept here");
    menu->set_hints({"A  Meaning", "X  Delete", "B  Close"});
    menu->set_uses_x(true);
    menu->set_on_selection([&state](uint32_t index) {
        const auto &list = vocabulary::entries();
        if (index < list.size())
        {
            state.view_stack.push(std::make_shared<DefinitionView>(
                list[index].word,
                list[index].gloss + (list[index].book.empty() ? std::string() : "\n\nFrom: " + list[index].book),
                state.sys_styling
            ));
        }
    });
    menu->set_default_on_keypress([fill](SDLKey key, SelectionMenu &m) {
        if (key != SW_BTN_X)
        {
            return;
        }
        const uint32_t index = m.get_cursor_pos();
        if (index >= vocabulary::entries().size())
        {
            return;
        }
        vocabulary::remove(index);
        fill(m);
        const size_t left = vocabulary::entries().size();
        m.set_cursor_pos(std::min<uint32_t>(index, left == 0 ? 0 : static_cast<uint32_t>(left) - 1));
    });
    state.view_stack.push(menu);
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

// Writes the highlights next to the book as a text file, which the book list can open too.
void export_highlights(ReaderViewState &state)
{
    const auto &highlights = state.token_view->highlights();
    if (highlights.empty())
    {
        show_message(state, "No highlights to export");
        return;
    }

    std::string name = state.book_title;
    for (char &c : name)
    {
        if (std::string("/\\:*?\"<>|").find(c) != std::string::npos)
        {
            c = '_';
        }
    }
    const std::filesystem::path out_path = state.book_path.parent_path() / (name + " - highlights.txt");

    std::ofstream out(out_path, std::ios::trunc);
    out << state.book_title << "\n\nHighlights\n\n";
    for (size_t index : highlight_order(highlights))
    {
        const Highlight &highlight = highlights[index];
        const int color = marker_color_of(highlight.style);
        out << percent_label(state, highlight.first) << "  "
            << (color > 0 ? std::string("[") + highlight_color_name(color) + "] " : std::string())
            << (highlight.text.empty() ? std::string("(text not saved)") : highlight.text) << "\n\n";
    }
    out.close();

    if (!out)
    {
        show_message(state, "Could not write the file");
        return;
    }
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Highlights exported",
        "Saved " + std::to_string(highlights.size()) + " highlights to\n" + out_path.string(),
        state.sys_styling
    ));
}

} // namespace

void show_definition(ReaderViewState &state, const std::string &lookup, const std::string &chosen = std::string());
void open_search_menu(ReaderView &reader_view, ReaderViewState &state);

void ReaderView::open_book_menu()
{
    struct Action
    {
        std::string name;
        std::function<void()> run;
    };
    auto actions = std::make_shared<std::vector<Action>>();

    if (!state->reader->get_table_of_contents().empty())
    {
        actions->push_back({"Chapters", [this]() { open_toc_menu(*this, *state); }});
    }
    actions->push_back({"Add bookmark", [this]() { add_bookmark(*state); }});
    actions->push_back({
        "Bookmarks (" + std::to_string(state->bookmarks.size()) + ")",
        [this]() { open_bookmarks_menu(*this, *state); }
    });
    if (!state->bookmarks.empty() || !state->token_view->highlights().empty())
    {
        actions->push_back({"Next mark", [this]() { go_to_mark(*this, *state, true); }});
        actions->push_back({"Previous mark", [this]() { go_to_mark(*this, *state, false); }});
    }
    actions->push_back({"Book map", [this]() {
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
    if (!state->reader->pages_are_pictures())
    {
        actions->push_back({"Search in book", [this]() { open_search_menu(*this, *state); }});
    }
    actions->push_back({state->reader->pages_are_pictures() ? "Browse pages" : "Skim the book", [this]() {
        state->view_stack.push(std::make_shared<PageBrowserView>(
            state->reader,
            get_current_address(*state),
            [this](DocAddr address) { jump_to(address); },
            state->sys_styling
        ));
    }});
    actions->push_back({"Go to %", [this]() {
        const uint32_t now = state->reader->get_global_progress_percent(get_current_address(*state));
        state->view_stack.push(std::make_shared<GotoView>(
            "Go to", "%", 0, 100, now,
            [this](uint32_t percent) { jump_to(state->reader->get_address_at_percent(percent)); },
            state->sys_styling
        ));
    }});
    {
        const bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
        uint32_t page = 0;
        uint32_t pages = 0;
        if (state->token_view_styling.get_reading_mode() == ReadingMode::Page
            && state->token_view->page_numbers(chapter, page, pages))
        {
            actions->push_back({"Go to page", [this, chapter, page, pages]() {
                state->view_stack.push(std::make_shared<GotoView>(
                    "Go to page of " + std::to_string(pages), "", 1, pages, page,
                    [this, chapter](uint32_t chosen) { go_to_page(chapter, chosen); },
                    state->sys_styling
                ));
            }});
        }
    }
    {
        const auto pictures = state->token_view->images_on_screen();
        if (!pictures.empty())
        {
            actions->push_back({
                pictures.size() == 1 ? std::string("View picture") : "View pictures (" + std::to_string(pictures.size()) + ")",
                [this, pictures]() { open_pictures(*state, pictures); }
            });
        }
    }
    if (can_jump_back())
    {
        actions->push_back({"Jump back", [this]() { jump_back(); }});
    }
    actions->push_back({"Sync progress", [this]() { open_sync_menu(*this, *state); }});
    actions->push_back({"Look up a word", [this]() {
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
    }});
    actions->push_back({
        "Word list (" + std::to_string(vocabulary::entries().size()) + ")",
        [this]() { open_word_list(*state); }
    });
    if (!vocabulary::entries().empty())
    {
        actions->push_back({"Export word list", [this]() { export_word_list(*state); }});
    }
    actions->push_back({"Reading history", [this]() {
        state->view_stack.push(std::make_shared<ReadingHistoryView>(
            state->sys_styling,
            [](const std::string &path) { return std::filesystem::path(path).stem().string(); }
        ));
    }});
    actions->push_back({"Reading stats", [this]() {
        state->view_stack.push(std::make_shared<DefinitionView>(
            "Reading stats",
            reading_stats::report(
                state->book_path,
                static_cast<long long>(std::time(nullptr)),
                library_log().started_count(),
                library_log().finished_count()
            ),
            state->sys_styling
        ));
    }});
    actions->push_back({
        "Highlights (" + std::to_string(state->token_view->highlights().size()) + ")",
        [this]() { open_highlights_menu(*this, *state); }
    });
    if (!state->token_view->highlights().empty())
    {
        actions->push_back({"Export highlights", [this]() { export_highlights(*state); }});
    }

    std::vector<std::string> names;
    for (const Action &action : *actions)
    {
        names.push_back(action.name);
    }

    auto menu = std::make_shared<SelectionMenu>(names, state->sys_styling);
    menu->set_title("Book");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([actions](uint32_t index) {
        if (index < actions->size())
        {
            (*actions)[index].run();
        }
    });
    menu->set_default_on_keypress([](SDLKey key, SelectionMenu &m) {
        if (key == SW_BTN_SELECT)
        {
            m.close();
        }
    });
    state->view_stack.push(menu);
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
            update_token_view_title(get_current_address(*state));
        }),
        view_stack,
        std::move(highlights_path)
    ))
{
    reading_speed::reset_position();
    reading_stats::reset();
    const auto book_info = read_epub_info(path);
    state->book_title = book_display_title(book_info, path);
    if (book_info)
    {
        state->dictionary_name = dictionary_name_for_language(book_info->language);
    }
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
            state->on_change_address(address);
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
    reading_speed::flush();
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

    bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
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
    // Time left from how fast pages have been going by.
    const auto book_extent = state->reader->get_progress_extent(address, false);
    reading_speed::note_position(book_extent.first, static_cast<long long>(std::time(nullptr)));
    const auto minutes_left = [](std::pair<uint32_t, uint32_t> extent) -> int {
        if (extent.second == 0)
        {
            return -1;
        }
        const uint32_t units = extent.second > extent.first ? extent.second - extent.first : 0;
        const long long seconds = reading_speed::seconds_for(units);
        return seconds < 0 ? -1 : static_cast<int>((seconds + 59) / 60);
    };
    const int chapter_minutes = minutes_left(state->reader->get_progress_extent(address, true));
    const int book_minutes = minutes_left(book_extent);

    state->token_view->set_title_progress(
        progress_percent, page, pages, chapter_percent, book_percent, chapter_minutes, book_minutes);
}

bool ReaderView::counting_pages() const
{
    return state->token_view_styling.get_reading_mode() == ReadingMode::Page && !pagination_ready();
}

bool ReaderView::pagination_ready() const
{
    bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
    return state->token_view->page_index_ready(chapter);
}

bool ReaderView::run_pagination(uint32_t budget_ms)
{
    if (state->token_view_styling.get_reading_mode() != ReadingMode::Page)
    {
        return false;
    }

    bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
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
        const bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
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
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Controls",
        controls_guide_text(),
        state.sys_styling
    ));
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

// Finds the word or phrase everywhere in the book and lists the places, with the chapter of each.
// The scan reads the whole book, so a bar shows how far it has got.
static void run_search(ReaderView &reader_view, ReaderViewState &state, const std::string &word)
{
    SystemStyling &styling = state.sys_styling;
    constexpr size_t MAX_HITS = 300;
    SearchOptions options;
    options.whole_word = (state.token_view_styling.get_search_options() & 1) != 0;
    options.match_case = (state.token_view_styling.get_search_options() & 2) != 0;
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
    menu->set_on_selection([&reader_view, &state, hits](uint32_t index) {
        if (index < hits->size())
        {
            state.token_view->cancel_word_select();
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
        std::string("Match case: ") + ((bits & 2) ? "On" : "Off")
    };
    std::vector<std::string> details = {"", "", ""};
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
        else if (index == 1 || index == 2)
        {
            state.token_view_styling.set_search_options(bits ^ (index == 1 ? 1 : 2));
            open_search_menu(reader_view, state);
        }
        else if (index - 3 < history.size())
        {
            run_search(reader_view, state, history[index - 3]);
        }
    });
    state.view_stack.push(menu);
}
// Looks the text up in the dictionary of the book language, falling back to English, and shows it.
// A phrase that has no entry of its own is looked up word by word.
void show_definition(ReaderViewState &state, const std::string &lookup, const std::string &chosen)
{
    static std::map<std::string, Dictionary> dictionaries;
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
        vocabulary::add(lookup, *gloss, state.book_title, now);
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
                    vocabulary::add(word, *part, state.book_title, now);
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
    state.view_stack.push(std::make_shared<DefinitionView>(
        lookup.empty() ? std::string("Look up") : lookup,
        body,
        state.sys_styling
    ));
}

static void open_word_actions(ReaderView &reader_view, ReaderViewState &state)
{
    std::string lookup = state.token_view->selected_lookup();
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
    menu->set_on_selection([&reader_view, &state, lookup, acts, target, dictionary_name = state.dictionary_name](uint32_t index) {
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
                show_definition(state, lookup);
                break;
            case Act::LookUpIn:
            {
                const std::vector<std::string> names = dictionary_names();
                auto pick = std::make_shared<SelectionMenu>(names, state.sys_styling);
                pick->set_title("Look up in");
                pick->set_compact(true);
                pick->set_close_on_select();
                pick->set_on_selection([&state, names, lookup](uint32_t chosen) {
                    if (chosen < names.size())
                    {
                        show_definition(state, lookup, names[chosen]);
                    }
                });
                state.view_stack.push(pick);
                break;
            }
            case Act::Highlight:
                state.token_view->toggle_selected_highlight(marker_style(state.token_view_styling.get_highlight_color()));
                break;
            case Act::Search:
                run_search(reader_view, state, lookup);
                break;
            case Act::Underline:
                state.token_view->toggle_selected_highlight(HighlightStyle::Underline);
                break;
            case Act::Strikeout:
                state.token_view->toggle_selected_highlight(HighlightStyle::Strikeout);
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
        state->token_view->on_keypress(key);
        return;
    }

    if (key == SW_BTN_START)
    {
        open_controls_guide(*state);
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

    switch (key) {
        case SW_BTN_A:
            state->token_view_styling.set_show_title_bar(
                !state->token_view_styling.get_show_title_bar()
            );
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

void ReaderView::on_keyheld(SDLKey key, uint32_t hold_time_ms)
{
    if (counting_pages())
    {
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
