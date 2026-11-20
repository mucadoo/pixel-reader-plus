#include "./reader_view.h"

#include "./definition_view.h"
#include "./selection_menu.h"
#include "reader/controls_guide.h"
#include "./token_view/token_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/dictionary.h"
#include "reader/reading_mode.h"
#include "reader/system_styling.h"
#include "reader/work_pump.h"
#include "reader/view_stack.h"

#include "doc_api/doc_reader.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"

#include <iostream>

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

    std::filesystem::path highlights_path;
    std::unique_ptr<TokenView> token_view;

    ReaderViewState(std::filesystem::path path, DocAddr seek_address, std::shared_ptr<DocReader> reader, SystemStyling &sys_styling, TokenViewStyling &token_view_styling, uint32_t token_view_styling_sub_id, ViewStack &view_stack, std::filesystem::path highlights_path)
        : filename(path.filename()),
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

} // namespace

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
    reader_clear_work();
    state->token_view_styling.unsubscribe_from_changes(
        state->token_view_styling_sub_id
    );
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

    uint32_t progress_percent = (
        chapter ?
        toc_position.progress_percent :
        state->reader->get_global_progress_percent(address)
    );

    uint32_t page = 0;
    uint32_t pages = 0;
    if (state->token_view_styling.get_reading_mode() == ReadingMode::Page)
    {
        state->token_view->page_numbers(chapter, page, pages);
    }
    state->token_view->set_title_progress(progress_percent, page, pages);
}

bool ReaderView::step_pagination()
{
    if (state->token_view_styling.get_reading_mode() != ReadingMode::Page)
    {
        return false;
    }

    bool chapter = state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT;
    bool was_ready = state->token_view->page_index_ready(chapter);
    if (was_ready)
    {
        return false;
    }

    // Count the whole book or chapter now, from its first line, then align.
    while (state->token_view->advance_page_index(chapter))
    {
    }
    if (state->token_view->page_index_ready(chapter))
    {
        update_token_view_title(get_current_address(*state));
        return true;
    }
    return false;
}

bool ReaderView::render(SDL_Surface *dest_surface, bool force_render)
{
    update_token_view_title(get_current_address(*state));
    return state->token_view->render(dest_surface, force_render);
}

bool ReaderView::is_done()
{
    return state->is_done;
}

static void open_controls_guide(ReaderViewState &state)
{
    state.view_stack.push(std::make_shared<DefinitionView>(
        "Controls",
        controls_guide_text(),
        state.sys_styling
    ));
}

static void open_word_actions(ReaderViewState &state)
{
    std::string lookup = state.token_view->selected_lookup();
    bool marked = state.token_view->selected_is_highlighted();
    std::vector<std::string> entries = {
        "Look up",
        marked ? "Remove highlight" : "Highlight"
    };
    auto menu = std::make_shared<SelectionMenu>(entries, state.sys_styling);
    menu->set_title(lookup.empty() ? std::string("Word") : lookup);
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&state, lookup](uint32_t index) {
        if (index == 0)
        {
            static Dictionary dictionary;
            std::string body;
            if (lookup.empty())
            {
                body = "Not a word";
            }
            else if (!dictionary.available())
            {
                body = "No dictionary installed";
            }
            else if (auto gloss = dictionary.lookup(lookup))
            {
                body = *gloss;
            }
            else
            {
                body = "No entry for \"" + lookup + "\"";
            }
            state.view_stack.push(std::make_shared<DefinitionView>(
                lookup.empty() ? std::string("Look up") : lookup,
                body,
                state.sys_styling
            ));
        }
        else
        {
            state.token_view->toggle_selected_highlight();
        }
    });
    state.view_stack.push(menu);
}

void ReaderView::on_keypress(SDLKey key)
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
            open_word_actions(*state);
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
            open_toc_menu(*this, *state);
            break;
        default:
            state->token_view->on_keypress(key);
            break;
    }
}

void ReaderView::on_keyheld(SDLKey key, uint32_t hold_time_ms)
{
    state->token_view->on_keyheld(key, hold_time_ms);
}

void ReaderView::set_on_change_address(std::function<void(DocAddr)> callback)
{
    state->on_change_address = callback;
}

void ReaderView::seek_to_toc_index(uint32_t toc_index)
{
    auto address = state->reader->get_toc_item_address(toc_index);
    seek_to_address(address);
}

void ReaderView::seek_to_address(DocAddr address)
{
    if (state->token_view)
    {
        state->token_view->seek_to_address(address);
        update_token_view_title(address);

        if (state->on_change_address)
        {
            state->on_change_address(address);
        }
    }
}
