#include "./config.h"
#include "./battery.h"
#include "./battery_estimate.h"
#include "./library_status.h"
#include "./book_layout.h"
#include "./views/popup_view.h"
#include "./reading_stats.h"
#include "./vocabulary.h"
#include "./work_pump.h"
#include "./font_catalog.h"
#include "./library_log.h"
#include "./reading_speed.h"
#include "./settings_store.h"
#include "./shoulder_keymap.h"
#include "./button_actions.h"
#include "./pending_progress.h"
#include "./dictionary.h"
#include "./state_store.h"
#include "./system_styling.h"
#include "./collections.h"
#include "./open_guard.h"
#include "./storage_usage.h"
#include "./views/selection_menu.h"
#include "./smart_collections.h"
#include "./color_theme_def.h"
#include <ctime>
#include "./view_stack.h"
#include "./views/definition_view.h"
#include "./kosync.h"
#include "./layout_profiles.h"
#include "./views/settings_menus.h"
#include "./views/book_grid.h"
#include "./views/file_selector.h"
#include "./views/reader_bootstrap_view.h"
#include "./views/image_view.h"
#include "./book_covers.h"
#include "./screen_dim.h"
#include "./layout_options.h"
#include "./views/font_preview_view.h"
#include "./views/settings_view.h"
#include "reader/controls_guide.h"
#include "./views/token_view/token_view_styling.h"
#include "filetypes/open_doc.h"
#include "reader/book_fonts.h"
#include "reader/folder_prefs.h"
#include "reader/next_book.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include <unistd.h>
#include "sys/trigger_input.h"
#include "util/fps_limiter.h"
#include "util/held_key_tracker.h"
#include "util/key_value_file.h"
#include "util/math.h"
#include "util/sdl_font_cache.h"
#include "util/task_queue.h"
#include "util/timer.h"

#include <libxml/parser.h>
#include <SDL/SDL.h>

#include <csignal>
#include <iostream>

namespace
{

// Where the browser starts, and the folder it may not leave.
struct BrowseLimits
{
    std::filesystem::path start;
    std::filesystem::path top_folder; // empty when there is no SD card to stay on
};

bool is_inside(const std::filesystem::path &path, const std::filesystem::path &folder)
{
    const auto p = path.lexically_normal();
    const auto f = folder.lexically_normal();
    auto p_it = p.begin();
    for (auto f_it = f.begin(); f_it != f.end(); ++f_it)
    {
        if (f_it->empty())
        {
            continue; // a trailing separator
        }
        if (p_it == p.end() || *p_it != *f_it)
        {
            return false;
        }
        ++p_it;
    }
    return true;
}

// The reader opens on the books folder of the SD card. It resumes the folder
// used last only while that is still on the card, so a first run, or a folder
// that is gone, never lands on the device's own file system.
BrowseLimits find_browse_limits(const std::optional<std::filesystem::path> &last_used)
{
    std::error_code ec;
    std::filesystem::path card;
    for (const char *candidate : {"/mnt/SDCARD", "/mnt/sdcard"})
    {
        if (std::filesystem::is_directory(candidate, ec))
        {
            card = candidate;
            break;
        }
    }

    if (card.empty())
    {
        return {last_used.value_or(DEFAULT_BROWSE_PATH), {}};
    }

    if (last_used && is_inside(*last_used, card))
    {
        return {*last_used, card};
    }

    std::filesystem::path books = card / "Roms" / "EBOOKS";
    if (!std::filesystem::is_directory(books, ec))
    {
        books = card;
    }
    return {books / "", card};
}

void initialize_views(
    ViewStack &view_stack,
    StateStore &state_store,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    TaskQueue &task_queue,
    const std::filesystem::path &store_path,
    std::optional<std::filesystem::path> requested_book_path
)
{
    auto load_book = [&view_stack, &state_store, &sys_styling, &token_view_styling, &task_queue](std::filesystem::path path) {
        if (!std::filesystem::exists(path))
        {
            std::cerr << path << " does not exist" << std::endl;
            return;
        }
        if (!file_type_is_supported(path))
        {
            std::cerr << path << " filetype is not supported" << std::endl;
            return;
        }

        view_stack.push(
            std::make_shared<ReaderBootstrapView>(
                path,
                sys_styling,
                token_view_styling,
                view_stack,
                state_store,
                [&task_queue](task_func task){ task_queue.submit(task); }
            )
        );
    };

    next_book::set_info_dir(store_path / "info");
    book_fonts::set_dir(store_path / "fonts");
    folder_prefs::set_file(store_path / "folders.tsv");
    next_book::set_opener(load_book);
    open_guard::set_file(store_path / "opening.txt");
    // The last run stopped while opening a book, or just after: do not walk into it again.
    const auto crash = requested_book_path ? std::nullopt : open_guard::take_crash(static_cast<long long>(std::time(nullptr)));

    if (requested_book_path)
    {
        load_book(*requested_book_path);
    }
    else
    {
        const BrowseLimits limits = find_browse_limits(state_store.get_current_browse_path());
        std::shared_ptr<FileSelector> fs = std::make_shared<FileSelector>(
            limits.start,
            sys_styling,
            limits.top_folder
        );

        fs->set_on_file_selected(load_book);
        fs->set_on_file_focus([&state_store](std::string path) {
            state_store.set_current_browse_path(path);
        });
        fs->set_on_view_focus([&state_store]() {
            state_store.remove_current_book_path();
            open_guard::end();
        });
        fs->set_on_open_view([&view_stack](std::shared_ptr<View> view) {
            view_stack.push(view);
        });
        fs->set_hide_finished(settings_get_hide_finished(state_store).value_or(false));
        fs->set_on_hide_finished_change([&state_store](bool hide) {
            settings_set_hide_finished(state_store, hide);
        });
        fs->set_on_info([&view_stack, &sys_styling](const std::string &title, const std::string &body) {
            view_stack.push(std::make_shared<DefinitionView>(title, body, sys_styling));
        });
        fs->set_on_open_at([&state_store, load_book](const std::filesystem::path &book, DocAddr address) {
            if (const auto *log = library_log().get(book))
            {
                if (!log->book_id.empty())
                {
                    state_store.set_book_address(log->book_id, address);
                }
            }
            load_book(book);
        });
        fs->set_on_book_deleted([&state_store](const std::string &book_id) {
            state_store.forget_book(book_id);
        });
        fs->set_flush_state([&state_store]() {
            state_store.flush();
            reading_speed::flush();
            reading_stats::flush();
            vocabulary::flush();
        });
        fs->set_cover_cache_dir(store_path / "covers");
        fs->set_info_cache_dir(store_path / "info");
        fs->set_sort(settings_get_browser_sort(state_store).value_or(BrowserSort::Name));
        fs->set_on_sort_change([&state_store](BrowserSort sort) {
            settings_set_browser_sort(state_store, sort);
        });
        fs->set_grid_view(settings_get_browser_grid(state_store).value_or(false));
        fs->set_on_view_mode_change([&state_store](bool grid) {
            settings_set_browser_grid(state_store, grid);
        });

        view_stack.push(fs);

        std::error_code crash_ec;
        if (crash && std::filesystem::exists(crash->book, crash_ec))
        {
            state_store.remove_current_book_path();
            const std::filesystem::path book = crash->book;
            auto menu = std::make_shared<SelectionMenu>(
                std::vector<std::string> {
                    "Go to the book list",
                    "Open it again",
                    "Clear saved page layouts, then open it",
                    "Open it plainly (no book styles, fonts or page numbers)"
                },
                sys_styling
            );
            menu->set_title(std::string(crash->while_opening ? "The reader stopped while opening " : "The reader stopped just after opening ") + book.stem().string());
            menu->set_compact(true);
            menu->set_close_on_select();
            menu->set_on_selection([book, load_book, &state_store](uint32_t choice) {
                if (choice >= 2)
                {
                    storage_usage::clear_files(state_store.book_data_dir(), ".cache", true);
                }
                if (choice == 3)
                {
                    open_guard::plain_next(book);
                }
                if (choice >= 1)
                {
                    load_book(book);
                }
            });
            view_stack.push(menu);
        }
        else if (state_store.get_current_book_path())
        {
            load_book(state_store.get_current_book_path().value());
        }
        else if (token_view_styling.get_startup() == 1)
        {
            // Start where the last session left off: the book read last, unless it is finished.
            if (const auto recent = library_log().most_recent())
            {
                const LibraryLog::Entry *log = library_log().get(*recent);
                std::error_code ec;
                if (!(log && log->finished()) && std::filesystem::exists(*recent, ec))
                {
                    load_book(*recent);
                }
            }
        }
    }
}

class SystemKeyChordTracker
{
    bool _menu_held = false;
    bool _select_held = false;

    bool _exit_on_menu_release = false;
    bool _exit_requested = false;

public:

    // Report keypress event. Return filtered key code.
    SDLKey on_keypress(SDLKey key)
    {
        // Block any other keys while special key is held
        SDLKey filtered_key = (_menu_held || _select_held) ? SDLK_UNKNOWN : key;

        if (key == SW_BTN_SELECT)
        {
            _select_held = true;
        }

        if (key == SW_BTN_MENU)
        {
            _menu_held = true;
            _exit_on_menu_release = true;
        }
        else
        {
            // Cancel app exit if a menu chord was used (e.g. change brightness)
            _exit_on_menu_release = false;
        }

        return filtered_key;
    }

    // Report keyrelease event.
    void on_keyrelease(SDLKey key)
    {
        if (key == SW_BTN_MENU)
        {
            _menu_held = false;

            if (_exit_on_menu_release)
            {
                _exit_requested = true;
            }
        }
        else if (key == SW_BTN_SELECT)
        {
            _select_held = false;
        }
    }

    bool exit_requested() const
    {
        return _exit_requested;
    }
};

bool quit = false;

void signal_handler(int)
{
    quit = true;
}

const char *CONFIG_KEY_STORE_PATH = "store_path";

std::unordered_map<std::string, std::string> load_config_with_defaults()
{
    auto config = load_key_value(CONFIG_FILE_PATH);
    config.try_emplace(CONFIG_KEY_STORE_PATH, FALLBACK_STORE_PATH);
    return config;
}

} // namespace

int main(int argc, char **argv)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (char* env_screen_width = SDL_getenv("SCREEN_WIDTH")) {
        int new_width = atoi(env_screen_width);
        if (100 < new_width && new_width < 4096)
            SCREEN_WIDTH = static_cast<unsigned int>(new_width);
    }

    if (char* env_screen_height = SDL_getenv("SCREEN_HEIGHT")) {
        int new_height = atoi(env_screen_height);
        if (100 < new_height && new_height < 4096)
            SCREEN_HEIGHT = static_cast<unsigned int>(new_height);
    }

    std::cout << "Screen Size: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;

    // SDL Init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();

    // Surfaces
    SDL_Surface *video = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_HWSURFACE);

    auto config = load_config_with_defaults();
    StateStore state_store(config[CONFIG_KEY_STORE_PATH]);

    // A turned screen is drawn at the swapped size and turned when it is shown.
    const int active_rotation = settings_get_small_int(state_store, "rotation").value_or(0);
    set_screen_rotation(active_rotation);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0);
    set_render_surface_format(screen->format);
    bool restart_requested = false;

    library_log().load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "library.log");
    reading_stats::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "stats.log");
    layout_profiles::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "profiles.tsv");
    collections::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "collections.tsv");
    smart_collections::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "smart_collections.tsv");
    load_dictionary_preferences((std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "dictionaries.txt").string());
    pending_progress::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "pending_progress.tsv");
    button_actions::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "buttons.txt");
    vocabulary::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "vocabulary.tsv");

    // Preload & check fonts
    auto init_font_name = get_valid_font_name(settings_get_font_name(state_store).value_or(DEFAULT_FONT_NAME));
    auto init_font_size = bound(settings_get_font_size(state_store).value_or(DEFAULT_FONT_SIZE), MIN_FONT_SIZE, MAX_FONT_SIZE);
    if (
        !cached_load_font(SYSTEM_FONT, init_font_size, FontLoadErrorOpt::NoThrow) ||
        !cached_load_font(init_font_name, init_font_size, FontLoadErrorOpt::NoThrow)
    )
    {
        std::cerr << "Failed to load one or more fonts" << std::endl;
        return 1;
    }

    // The colors of the custom theme, three RRGGBB codes
    if (const auto colors = settings_get_custom_colors(state_store); colors && colors->size() == 20)
    {
        const auto read = [&](size_t at, SDL_Color &out) {
            unsigned int value = 0;
            if (std::sscanf(colors->c_str() + at, "%6x", &value) != 1)
            {
                return false;
            }
            out = {static_cast<Uint8>(value >> 16), static_cast<Uint8>((value >> 8) & 0xFF), static_cast<Uint8>(value & 0xFF), 0};
            return true;
        };
        SDL_Color background, text, highlight;
        if (read(0, background) && read(7, text) && read(14, highlight))
        {
            set_custom_theme(background, text, highlight);
        }
    }

    // System styling
    SystemStyling sys_styling(
        init_font_name,
        init_font_size,
        get_valid_theme(settings_get_color_theme(state_store).value_or(DEFAULT_COLOR_THEME)),
        get_valid_shoulder_keymap(settings_get_shoulder_keymap(state_store).value_or(DEFAULT_SHOULDER_KEYMAP))
    );
    // True while the automatic night theme is being applied, so it is not saved as the chosen theme.
    bool applying_night_theme = false;
    sys_styling.subscribe_to_changes([&state_store, &sys_styling, &applying_night_theme](SystemStyling::ChangeId) {
        // Persist changes
        if (!applying_night_theme)
        {
            settings_set_color_theme(state_store, sys_styling.get_color_theme());
        }
        if (custom_theme_chosen())
        {
            SDL_Color background, text, highlight;
            get_custom_theme(background, text, highlight);
            char colors[24];
            std::snprintf(colors, sizeof(colors), "%02X%02X%02X,%02X%02X%02X,%02X%02X%02X",
                background.r, background.g, background.b, text.r, text.g, text.b, highlight.r, highlight.g, highlight.b);
            settings_set_custom_colors(state_store, colors);
        }
        settings_set_shoulder_keymap(state_store, sys_styling.get_shoulder_keymap());
        settings_set_small_int(state_store, "text_contrast", sys_styling.get_text_contrast());
        if (book_layout::active())
        {
            // Inside a book the font and size belong to that book alone.
            book_layout::save_if_active();
        }
        else
        {
            settings_set_font_name(state_store, sys_styling.get_font_name());
            settings_set_font_size(state_store, sys_styling.get_font_size());
        }
    });

    sys_styling.set_text_contrast(settings_get_small_int(state_store, "text_contrast").value_or(0));

    // Text Styling
    TokenViewStyling token_view_styling(
        settings_get_show_title_bar(state_store).value_or(DEFAULT_SHOW_PROGRESS),
        settings_get_progress_reporting(state_store).value_or(DEFAULT_PROGRESS_REPORTING),
        settings_get_reading_mode(state_store).value_or(DEFAULT_READING_MODE)
    );
    token_view_styling.set_bar_left(settings_get_bar_item(state_store, 0).value_or(token_view_styling.get_bar_left()));
    token_view_styling.set_bar_middle(settings_get_bar_item(state_store, 1).value_or(token_view_styling.get_bar_middle()));
    token_view_styling.set_bar_right(settings_get_bar_item(state_store, 2).value_or(token_view_styling.get_bar_right()));
    token_view_styling.set_clock_format(settings_get_clock_format(state_store).value_or(token_view_styling.get_clock_format()));
    token_view_styling.set_margin_level(settings_get_margin_level(state_store).value_or(token_view_styling.get_margin_level()));
    token_view_styling.set_spacing_level(settings_get_spacing_level(state_store).value_or(token_view_styling.get_spacing_level()));
    token_view_styling.set_justify(settings_get_justify(state_store).value_or(token_view_styling.get_justify()));
    token_view_styling.set_hyphenate(settings_get_hyphenate(state_store).value_or(token_view_styling.get_hyphenate()));
    token_view_styling.set_indent_level(settings_get_indent_level(state_store).value_or(token_view_styling.get_indent_level()));
    token_view_styling.set_paragraph_gap(settings_get_paragraph_gap(state_store).value_or(token_view_styling.get_paragraph_gap()));
    token_view_styling.set_bold_text(settings_get_bold_text(state_store).value_or(token_view_styling.get_bold_text()));
    token_view_styling.set_picture_mode(settings_get_picture_mode(state_store).value_or(0));
    token_view_styling.set_thin_bar(settings_get_thin_bar(state_store).value_or(false));
    token_view_styling.set_scroll_overlap(settings_get_small_int(state_store, "scroll_overlap").value_or(0));
    kosync::set_config_path(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "sync");
    token_view_styling.set_margin_right(settings_get_small_int(state_store, "margin_right").value_or(token_view_styling.get_margin_level()));
    token_view_styling.set_margin_top(settings_get_small_int(state_store, "margin_top").value_or(0));
    token_view_styling.set_margin_bottom(settings_get_small_int(state_store, "margin_bottom").value_or(0));
    token_view_styling.set_ruler(settings_get_small_int(state_store, "ruler").value_or(0));
    token_view_styling.set_letter_spacing(settings_get_small_int(state_store, "letter_spacing").value_or(0));
    token_view_styling.set_rotation(active_rotation);
    if (const auto saved_tweaks = state_store.get_setting("tweaks"))
    {
        token_view_styling.set_tweaks(std::atoi(saved_tweaks->c_str()));
    }
    token_view_styling.set_grid_size(settings_get_small_int(state_store, "grid_size").value_or(0));
    token_view_styling.set_startup(settings_get_small_int(state_store, "startup").value_or(0));
    token_view_styling.set_library_status(settings_get_small_int(state_store, "library_status").value_or(0));
    library_status::configure(token_view_styling.get_library_status(), token_view_styling.get_clock_format() == ClockFormat::H12);
    token_view_styling.set_low_battery_warning(settings_get_small_int(state_store, "low_battery").value_or(1) != 0);
    BookGrid::set_density(token_view_styling.get_grid_size());
    token_view_styling.set_word_spacing(settings_get_small_int(state_store, "word_spacing").value_or(1));
    token_view_styling.set_cover_sleep(settings_get_small_int(state_store, "cover_sleep").value_or(0));
    token_view_styling.set_screen_dim(settings_get_small_int(state_store, "screen_dim").value_or(0));
    screen_dim::set_level(token_view_styling.get_screen_dim());
    token_view_styling.set_book_styles(settings_get_small_int(state_store, "book_styles").value_or(1) != 0);
    token_view_styling.set_highlight_color(settings_get_small_int(state_store, "highlight_color").value_or(0));
    token_view_styling.set_search_options(settings_get_small_int(state_store, "search_options").value_or(1));
    token_view_styling.set_search_history(state_store.get_setting("search_history").value_or(std::string()));
    token_view_styling.set_dictionary(settings_get_dictionary(state_store).value_or(std::string()));
    token_view_styling.set_night_theme(settings_get_night_theme(state_store).value_or(std::string()));
    token_view_styling.set_night_from(settings_get_night_hour(state_store, true).value_or(21));
    token_view_styling.set_night_until(settings_get_night_hour(state_store, false).value_or(7));
    token_view_styling.set_auto_exit_minutes(settings_get_auto_exit_minutes(state_store).value_or(0));
    book_layout::attach(token_view_styling);
    book_layout::attach_system(sys_styling);
    if (const auto speed = settings_get_reading_speed(state_store))
    {
        reading_speed::set_units_per_second(*speed);
    }
    reading_speed::set_saver([&state_store](double speed) {
        settings_set_reading_speed(state_store, speed);
    });
    token_view_styling.subscribe_to_changes([&token_view_styling, &state_store]() {
        // Persist changes
        library_status::configure(token_view_styling.get_library_status(), token_view_styling.get_clock_format() == ClockFormat::H12);
        settings_set_show_title_bar(state_store, token_view_styling.get_show_title_bar());
        settings_set_progress_reporting(state_store, token_view_styling.get_progress_reporting());
        if (book_layout::active())
        {
            // Inside a book these belong to that book alone.
            book_layout::save_if_active();
        }
        else
        {
            settings_set_reading_mode(state_store, token_view_styling.get_reading_mode());
            settings_set_margin_level(state_store, token_view_styling.get_margin_level());
            settings_set_spacing_level(state_store, token_view_styling.get_spacing_level());
            settings_set_justify(state_store, token_view_styling.get_justify());
            settings_set_hyphenate(state_store, token_view_styling.get_hyphenate());
            settings_set_picture_mode(state_store, token_view_styling.get_picture_mode());
            settings_set_thin_bar(state_store, token_view_styling.get_thin_bar());
            settings_set_small_int(state_store, "scroll_overlap", token_view_styling.get_scroll_overlap());
            settings_set_small_int(state_store, "margin_right", token_view_styling.get_margin_right());
            settings_set_small_int(state_store, "margin_top", token_view_styling.get_margin_top());
            settings_set_small_int(state_store, "margin_bottom", token_view_styling.get_margin_bottom());
            settings_set_small_int(state_store, "ruler", token_view_styling.get_ruler());
            settings_set_small_int(state_store, "letter_spacing", token_view_styling.get_letter_spacing());
            settings_set_small_int(state_store, "grid_size", token_view_styling.get_grid_size());
            settings_set_small_int(state_store, "startup", token_view_styling.get_startup());
            settings_set_small_int(state_store, "library_status", token_view_styling.get_library_status());
            settings_set_small_int(state_store, "low_battery", token_view_styling.get_low_battery_warning() ? 1 : 0);
            BookGrid::set_density(token_view_styling.get_grid_size());
            settings_set_small_int(state_store, "word_spacing", token_view_styling.get_word_spacing());
            settings_set_small_int(state_store, "cover_sleep", token_view_styling.get_cover_sleep());
            settings_set_small_int(state_store, "screen_dim", token_view_styling.get_screen_dim());
            screen_dim::set_level(token_view_styling.get_screen_dim());
            settings_set_small_int(state_store, "book_styles", token_view_styling.get_book_styles() ? 1 : 0);
            settings_set_small_int(state_store, "highlight_color", token_view_styling.get_highlight_color());
            settings_set_small_int(state_store, "search_options", token_view_styling.get_search_options());
            state_store.set_setting("search_history", token_view_styling.get_search_history());
            settings_set_dictionary(state_store, token_view_styling.get_dictionary());
            settings_set_night_theme(state_store, token_view_styling.get_night_theme());
            settings_set_night_hour(state_store, true, token_view_styling.get_night_from());
            settings_set_night_hour(state_store, false, token_view_styling.get_night_until());
            settings_set_indent_level(state_store, token_view_styling.get_indent_level());
            settings_set_paragraph_gap(state_store, token_view_styling.get_paragraph_gap());
            settings_set_bold_text(state_store, token_view_styling.get_bold_text());
        }
        settings_set_auto_exit_minutes(state_store, token_view_styling.get_auto_exit_minutes());
        settings_set_small_int(state_store, "rotation", token_view_styling.get_rotation());
        state_store.set_setting("tweaks", std::to_string(token_view_styling.get_tweaks()));
        settings_set_bar_item(state_store, 0, token_view_styling.get_bar_left());
        settings_set_bar_item(state_store, 1, token_view_styling.get_bar_middle());
        settings_set_bar_item(state_store, 2, token_view_styling.get_bar_right());
        settings_set_clock_format(state_store, token_view_styling.get_clock_format());
    });

    // Setup views
    TaskQueue task_queue;
    ViewStack view_stack;

    std::optional<std::filesystem::path> requested_book_path = (
        argc == 2 ? std::optional<std::filesystem::path>(argv[1]) : std::nullopt
    );
    initialize_views(
        view_stack,
        state_store,
        sys_styling,
        token_view_styling,
        task_queue,
        config[CONFIG_KEY_STORE_PATH],
        requested_book_path
    );
    quit = view_stack.is_done();

    std::shared_ptr<SettingsView> settings_view = std::make_shared<SettingsView>(
        sys_styling,
        token_view_styling,
        SYSTEM_FONT
    );
    settings_view->set_row_action(SettingsView::TWEAKS_ROW, [&view_stack, &sys_styling, &token_view_styling]() {
        open_style_tweaks_menu(view_stack, sys_styling, token_view_styling);
    });
    settings_view->set_row_action(SettingsView::TYPOGRAPHY_ROW, [&view_stack, &sys_styling, &token_view_styling]() {
        open_typography_menu(view_stack, sys_styling, token_view_styling);
    });
    settings_view->set_row_action(SettingsView::CUSTOM_COLORS_ROW, [&view_stack, &sys_styling]() {
        open_custom_colors_menu(view_stack, sys_styling);
    });
    settings_view->set_row_action(SettingsView::PROFILES_ROW, [&view_stack, &sys_styling, &token_view_styling]() {
        open_layout_profiles_menu(view_stack, sys_styling, token_view_styling);
    });
    // A on Font or Font size shows a sample paragraph, to choose both by sight.
    for (const uint32_t row : {1u, 2u})
    {
        settings_view->set_row_action(row, [&view_stack, &sys_styling]() {
            view_stack.push(std::make_shared<FontPreviewView>(sys_styling));
        });
    }
    settings_view->set_row_action(SettingsView::DICTIONARY_ORDER_ROW, [&view_stack, &sys_styling]() {
        open_dictionary_order_menu(view_stack, sys_styling);
    });
    settings_view->set_row_action(SettingsView::BUTTONS_ROW, [&view_stack, &sys_styling]() {
        open_button_actions_menu(view_stack, sys_styling);
    });
    settings_view->set_controls_action([&view_stack, &sys_styling]() {
        view_stack.push(std::make_shared<DefinitionView>(
            "Controls",
            controls_guide_text(),
            sys_styling
        ));
    });

    // Track held keys
    HeldKeyTracker held_key_tracker(
        {
            SW_BTN_UP,
            SW_BTN_DOWN,
            SW_BTN_LEFT,
            SW_BTN_RIGHT,
            SW_BTN_L1,
            SW_BTN_R1,
            SW_BTN_L2,
            SW_BTN_R2,
            // held for the hold actions of the button settings
            SW_BTN_A,
            SW_BTN_Y,
            SW_BTN_START
        }
    );
    SystemKeyChordTracker chord_tracker;

    TriggerInput trigger_input;
    auto is_trigger = [](SDLKey key) {
        return key == SW_BTN_L2 || key == SW_BTN_R2;
    };
    held_key_tracker.set_device_key(device_key_from_screen);
    held_key_tracker.set_held_override([&trigger_input, &is_trigger](SDLKey key) -> std::optional<bool> {
        if (trigger_input.active() && is_trigger(key))
        {
            return trigger_input.is_down(key);
        }
        return std::nullopt;
    });

    auto key_held_callback = [&view_stack](SDLKey key, uint32_t held_ms) {
        view_stack.on_keyheld(key, held_ms);
    };

    // Timing
    Timer idle_timer;
    Timer save_timer;
    bool cover_shown = false; // the cover screen is up until a key is pressed
    Timer quiet_timer; // time since the last key press, for Auto exit
    Timer night_timer;
    Timer battery_timer;
    int battery_warned = 100; // the lowest level a warning was shown for since the last charge
    bool night_active = false;
    std::string day_theme;
    std::string night_theme_applied;

    // Switches to the night theme when the night hours begin, and back when they end.
    auto update_night_theme = [&]() {
        const std::string night = token_view_styling.get_night_theme();
        bool is_night = false;
        if (!night.empty())
        {
            const std::time_t now = std::time(nullptr);
            std::tm local {};
            if (localtime_r(&now, &local))
            {
                const int from = token_view_styling.get_night_from();
                const int until = token_view_styling.get_night_until();
                const int hour = local.tm_hour;
                if (from != until)
                {
                    is_night = from < until ? (hour >= from && hour < until) : (hour >= from || hour < until);
                }
            }
        }
        if (is_night && !night_active)
        {
            day_theme = sys_styling.get_color_theme();
            night_active = true;
            if (day_theme != night)
            {
                applying_night_theme = true;
                sys_styling.set_color_theme(night);
                applying_night_theme = false;
            }
        }
        else if (!is_night && night_active)
        {
            night_active = false;
            // A theme chosen by hand during the night is kept.
            if (sys_styling.get_color_theme() == night_theme_applied && !day_theme.empty())
            {
                sys_styling.set_color_theme(day_theme);
            }
        }
        if (night_active)
        {
            night_theme_applied = night;
        }
    };
    update_night_theme();
    night_timer.reset();

    // Returns true when the key ran view code.
    auto handle_key_down = [&](SDLKey raw_key) {
        idle_timer.reset();
        quiet_timer.reset();
        cover_shown = false;

        SDLKey key = chord_tracker.on_keypress(raw_key);

        if (key == SW_BTN_POWER)
        {
            state_store.flush();
            return false;
        }

        const auto top_before = view_stack.top_view();
        const bool x_taken = top_before && top_before->consumes_x();
        view_stack.on_keypress(key);

        auto top = view_stack.top_view();
        if (key == SW_BTN_X && !x_taken && top && !top->is_done())
        {
            if (top != settings_view)
            {
                settings_view->unterminate();
                view_stack.push(settings_view);
            }
            else
            {
                settings_view->terminate();
            }
        }
        return true;
    };
    FPSLimiter limit_fps(TARGET_FPS);
    const uint32_t avg_loop_time = 1000 / TARGET_FPS;

    // Initial render
    view_stack.render(screen, true);
    present_screen(screen, video);
    screen_dim::apply(video);
    SDL_Flip(video);

    while (!quit)
    {
        bool ran_user_code = task_queue.drain();
        if (reader_step_work())
        {
            ran_user_code = true;
        }

        trigger_input.poll([&](SDLKey key, bool down) {
            if (down)
            {
                ran_user_code = handle_key_down(key) || ran_user_code;
            }
            else
            {
                chord_tracker.on_keyrelease(key);
            }
        });

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                    {
                        SDLKey key = event.key.keysym.sym;
                        if (trigger_input.active() && is_trigger(key))
                        {
                            break; // read from the pad instead
                        }
                        ran_user_code = handle_key_down(screen_key_from_device(key)) || ran_user_code;
                    }
                    break;
                case SDL_KEYUP:
                    {
                        SDLKey key = event.key.keysym.sym;
                        if (trigger_input.active() && is_trigger(key))
                        {
                            break;
                        }
                        chord_tracker.on_keyrelease(screen_key_from_device(key));
                        view_stack.on_keyrelease(screen_key_from_device(key));
                        ran_user_code = true;
                    }
                    break;
                default:
                    break;
            }
        }

        quit = quit || chord_tracker.exit_requested();

        // A new screen turn takes effect once the settings are closed, by starting the reader again.
        if (!quit && token_view_styling.get_rotation() != active_rotation && settings_view->is_done())
        {
            restart_requested = true;
            quit = true;
        }

        held_key_tracker.accumulate(avg_loop_time); // Pretend perfect loop timing for event firing consistency
        ran_user_code = held_key_tracker.for_newest_held(key_held_callback) || ran_user_code;
        ran_user_code = view_stack.tick(avg_loop_time) || ran_user_code;

        if (ran_user_code)
        {
            bool force_render = view_stack.pop_completed_views();
            if (view_stack.top_view() == nullptr || force_render)
            {
                // A reader that closed to hand over to the next book of a series.
                next_book::open_requested();
            }

            if (view_stack.is_done())
            {
                quit = true;
            }

            if (view_stack.render(screen, force_render))
            {
                present_screen(screen, video);
                screen_dim::apply(video);
                SDL_Flip(video);
            }
        }

        if (!quit)
        {
            limit_fps();
        }

        if (idle_timer.elapsed_sec() >= IDLE_SAVE_TIME_SEC || save_timer.elapsed_sec() >= IDLE_SAVE_TIME_SEC)
        {
            // Make sure state is saved in case device auto-powers down, or the
            // battery runs out while reading. Don't seem to get a signal on
            // miyoo mini when this happens.
            state_store.flush();
            library_log().flush();
            reading_speed::flush();
            reading_stats::flush();
            idle_timer.reset();
            save_timer.reset();
        }

        if (night_timer.elapsed_sec() >= 5)
        {
            update_night_theme();
            night_timer.reset();
        }

        if (battery_timer.elapsed_sec() >= 30)
        {
            battery_timer.reset();
            const int level = battery_percent();
            battery_estimate::note(static_cast<long long>(std::time(nullptr)), level, battery_charging());
            if (level < 0 || battery_charging() || level > 20)
            {
                battery_warned = 100;
            }
            else if (token_view_styling.get_low_battery_warning())
            {
                int threshold = 0;
                if (level <= 5 && battery_warned > 5)
                {
                    threshold = 5;
                }
                else if (level <= 15 && battery_warned > 15)
                {
                    threshold = 15;
                }
                if (threshold > 0)
                {
                    battery_warned = threshold;
                    if (threshold == 5)
                    {
                        // Make sure nothing is lost if the device switches off.
                        state_store.flush();
                        library_log().flush();
                        reading_speed::flush();
                        reading_stats::flush();
                    }
                    view_stack.push(std::make_shared<PopupView>(
                        std::string("Battery low: ") + std::to_string(level) + "%"
                            + (threshold == 5 ? "\nYour place is saved. Charge soon." : "\nCharge soon."),
                        sys_styling.get_font_name(),
                        sys_styling
                    ));
                    view_stack.render(screen, true);
                    present_screen(screen, video);
                    screen_dim::apply(video);
                    SDL_Flip(video);
                }
            }
        }

        const int cover_minutes = cover_sleep_minutes(token_view_styling.get_cover_sleep());
        if (cover_minutes > 0 && !cover_shown && quiet_timer.elapsed_sec() >= static_cast<uint32_t>(cover_minutes) * 60u)
        {
            cover_shown = true;
            if (const auto open_book = state_store.get_current_book_path())
            {
                std::vector<char> bytes;
                std::string format;
                if (read_book_cover(*open_book, bytes, format))
                {
                    auto cover = std::make_shared<ImageView>(bytes, format, sys_styling, 0);
                    if (cover->valid())
                    {
                        cover->set_screensaver();
                        view_stack.push(cover);
                        view_stack.render(screen, true);
                        present_screen(screen, video);
                        screen_dim::apply(video);
                        SDL_Flip(video);
                    }
                }
            }
        }

        const int auto_exit = token_view_styling.get_auto_exit_minutes();
        if (auto_exit > 0 && quiet_timer.elapsed_sec() >= static_cast<uint32_t>(auto_exit) * 60u)
        {
            quit = true; // the normal exit below saves everything
        }
    }

    view_stack.shutdown();
    open_guard::end();
    state_store.flush();
    reading_speed::flush();
    library_log().flush();
    reading_stats::flush();

    SDL_FreeSurface(screen);
    SDL_Quit();
    xmlCleanupParser();

    if (restart_requested)
    {
        execv("/proc/self/exe", argv);
    }

    return 0;
}
