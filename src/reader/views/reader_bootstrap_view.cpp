#include "./reader_bootstrap_view.h"
#include "reader/pending_progress.h"
#include "reader/book_fonts.h"
#include "reader/open_guard.h"

#include "./popup_view.h"
#include "./reader_view.h"
#include "doc_api/open_progress.h"
#include "./token_view/token_view_styling.h"
#include "filetypes/epub/book_style.h"
#include "filetypes/open_doc.h"
#include "reader/progress_screen.h"
#include "reader/book_layout.h"
#include "reader/config.h"
#include "reader/library_log.h"
#include "reader/ss_doc_reader_cache.h"
#include "reader/state_store.h"
#include "reader/system_styling.h"
#include "reader/view_stack.h"
#include "sys/screen.h"

#include <ctime>
#include <iostream>

struct ReaderBootstrapViewState
{
    std::filesystem::path book_path;
    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    ViewStack &view_stack;
    StateStore &state_store;

    bool is_done = false;
    bool needs_render = true;

    ReaderBootstrapViewState(
        std::filesystem::path book_path,
        SystemStyling &sys_styling,
        TokenViewStyling &token_view_styling,
        ViewStack &view_stack,
        StateStore &state_store
    ) :
        book_path(book_path),
        sys_styling(sys_styling),
        token_view_styling(token_view_styling),
        view_stack(view_stack),
        state_store(state_store)
    {
    }
};

void ReaderBootstrapView::load_reader()
{
    state->is_done = true;

    auto &book_path = state->book_path;
    auto &sys_styling = state->sys_styling;
    auto &token_view_styling = state->token_view_styling;
    auto &view_stack = state->view_stack;
    auto &state_store = state->state_store;

    // After a stop while opening this book the user may have asked for it plain: no book styles, fonts or page numbers.
    const bool plain = open_guard::take_plain(book_path);
    int tweaks = token_view_styling.get_tweaks();
    if (plain)
    {
        tweaks |= TWEAK_PLAIN_BREAKS | TWEAK_PLAIN_SIZES | TWEAK_PLAIN_CENTER | TWEAK_PLAIN_FONTS | TWEAK_PLAIN_PAGES;
    }
    open_guard::begin(book_path, static_cast<long long>(std::time(nullptr)));
    set_book_styles_enabled(plain ? false : token_view_styling.get_book_styles());
    set_style_tweaks(tweaks);
    std::shared_ptr<DocReader> reader = create_doc_reader(book_path);
    SSDocReaderCache cache(state_store);
    // The first time a book is opened every part of it is measured. Show how far that is.
    const std::string shown_name = book_path.stem().string();
    set_open_progress_hook([&sys_styling, shown_name](uint32_t done, uint32_t total) {
        progress_screen::show(sys_styling, "Opening book", shown_name, total ? static_cast<int>(done * 100 / total) : 0);
    });
    const bool opened = reader && reader->open(cache);
    set_open_progress_hook(nullptr);
    if (!opened)
    {
        std::cerr << "Failed to open " << book_path << std::endl;
        open_guard::end();
        view_stack.push(std::make_shared<PopupView>("Error opening", SYSTEM_FONT, sys_styling));
        return;
    }

    state_store.set_current_book_path(book_path);

    book_fonts::open(*reader, reader->get_id(), !(tweaks & TWEAK_PLAIN_FONTS));
    if (!(tweaks & TWEAK_PLAIN_PAGES))
    {
        // Finds where the printed pages of the book start, which reads parts of it.
        progress_screen::show(sys_styling, "Opening book", shown_name, 100);
        reader->page_list();
    }

    auto book_id = reader->get_id();
    DocAddr resume_address = state_store.get_book_address(book_id).value_or(0);
    if (const auto set_by_hand = pending_progress::take(book_path))
    {
        // A place chosen in the library: open there.
        resume_address = reader->get_address_at_percent(static_cast<uint32_t>(*set_by_hand));
        state_store.set_book_address(book_id, resume_address);
    }
    library_log().set_book_id(book_path, book_id);
    library_log().set_units(book_path, reader->get_progress_extent(0, false).second);
    library_log().touch(book_path, static_cast<long long>(std::time(nullptr)));
    library_log().set_percent(book_path, static_cast<int>(reader->get_global_progress_percent(resume_address)));
    book_layout::open(state_store.book_data_dir() / (book_id + ".layout"));
    auto reader_view = std::make_shared<ReaderView>(
        book_path,
        reader,
        state_store.get_book_address(book_id).value_or(0),
        sys_styling,
        token_view_styling,
        view_stack,
        state_store.book_data_dir() / (book_id + ".highlights")
    );

    reader_view->set_on_change_address([&state_store, book_id, book_path, reader](DocAddr addr) {
        state_store.set_book_address(book_id, addr);
        library_log().set_percent(book_path, static_cast<int>(reader->get_global_progress_percent(addr)));
    });

    view_stack.push(reader_view);
    open_guard::opened(static_cast<long long>(std::time(nullptr)));
}

ReaderBootstrapView::ReaderBootstrapView(
    std::filesystem::path book_path,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    ViewStack &view_stack,
    StateStore &state_store,
    std::function<void(std::function<void()>)> async
) : state(std::make_unique<ReaderBootstrapViewState>(book_path, sys_styling, token_view_styling, view_stack, state_store))
{
    // Perform asynchronously so that rendering can continue
    async([this](){ load_reader(); });
}

ReaderBootstrapView::~ReaderBootstrapView()
{
}

bool ReaderBootstrapView::render(SDL_Surface *dest_surface, bool force_render)
{
    bool perform_render = force_render || state->needs_render;
    if (perform_render)
    {
        // blank screen during loading
        const auto &bg_color = state->sys_styling.get_loaded_color_theme().background;

        SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        uint32_t surf_color = SDL_MapRGB(
            dest_surface->format,
            bg_color.r,
            bg_color.g,
            bg_color.b
        );
        SDL_FillRect(dest_surface, &rect, surf_color);

        state->needs_render = false;
    }

    return perform_render;
}

bool ReaderBootstrapView::is_done()
{
    return state->is_done;
}

void ReaderBootstrapView::on_keypress(SDLKey)
{
}
