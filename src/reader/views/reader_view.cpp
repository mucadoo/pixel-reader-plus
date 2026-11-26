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
#include "reader/library_log.h"
#include "reader/reading_speed.h"
#include "reader/book_search.h"
#include "reader/reading_stats.h"
#include "reader/vocabulary.h"
#include "reader/reading_mode.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "reader/work_pump.h"
#include "reader/view_stack.h"

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

void open_toc_menu(ReaderView &reader_view, ReaderViewState &state)
{
    const auto &toc = state.reader->get_table_of_contents();
    if (toc.empty())
    {
        return;
    }

    // setup toc entries & callbacks
    std::vector<std::string> menu_names;
    for (const auto &toc_item: toc)
    {
        std::string indent(toc_item.indent_level * 2, ' ');
        menu_names.push_back(indent + toc_item.display_name);
    }

    auto current_toc_index = state.reader->get_toc_position(get_current_address(state)).toc_index;
    auto toc_select_menu = std::make_shared<SelectionMenu>(
        menu_names,
        state.sys_styling
    );
    toc_select_menu->set_on_selection([&reader_view, last_toc_index=current_toc_index](uint32_t toc_index) {
        if (toc_index != last_toc_index)
        {
            reader_view.seek_to_toc_index(toc_index);
        }
    });
    toc_select_menu->set_close_on_select();
    toc_select_menu->set_title("Chapters");

    // select current toc item
    if (current_toc_index < toc.size())
    {
        toc_select_menu->set_marker(current_toc_index);
        toc_select_menu->set_cursor_pos(current_toc_index);
    }

    toc_select_menu->set_default_on_keypress([](SDLKey key, SelectionMenu &toc) {
        if (key == SW_BTN_SELECT)
        {
            toc.close();
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
    actions->push_back({"Look up a word", [this]() {
        state->view_stack.push(std::make_shared<TextInputView>(
            "Look up a word",
            "",
            [this](const std::string &typed) {
                std::string word = typed;
                while (!word.empty() && word.back() == ' ') word.pop_back();
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

// Finds the word everywhere in the book and lists the places. The scan reads the
// whole book, so a bar shows how far it has got.
static void run_search(ReaderView &reader_view, ReaderViewState &state, const std::string &word)
{
    SystemStyling &styling = state.sys_styling;
    constexpr size_t MAX_HITS = 300;
    const auto hits = std::make_shared<std::vector<SearchHit>>(search_book(
        *state.reader,
        word,
        MAX_HITS,
        [&styling, &word](int percent) {
            progress_screen::show(styling, "Searching", word, percent);
        }
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
    for (const SearchHit &hit : *hits)
    {
        names.push_back(hit.snippet);
        details.push_back(percent_label(state, hit.address));
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
