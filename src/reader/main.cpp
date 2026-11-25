#include "./config.h"
#include "./book_layout.h"
#include "./reading_stats.h"
#include "./vocabulary.h"
#include "./work_pump.h"
#include "./font_catalog.h"
#include "./library_log.h"
#include "./reading_speed.h"
#include "./settings_store.h"
#include "./shoulder_keymap.h"
#include "./state_store.h"
#include "./system_styling.h"
#include "./collections.h"
#include "./color_theme_def.h"
#include <ctime>
#include "./view_stack.h"
#include "./views/definition_view.h"
#include "./views/file_selector.h"
#include "./views/reader_bootstrap_view.h"
#include "./views/image_view.h"
#include "./book_covers.h"
#include "./layout_options.h"
#include "./views/settings_view.h"
#include "reader/controls_guide.h"
#include "./views/token_view/token_view_styling.h"
#include "filetypes/open_doc.h"
#include "sys/keymap.h"
#include "sys/screen.h"
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
        fs->set_on_book_deleted([&state_store](const std::string &book_id) {
            state_store.forget_book(book_id);
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

        if (state_store.get_current_book_path())
        {
            load_book(state_store.get_current_book_path().value());
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
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0);
    set_render_surface_format(screen->format);

    auto config = load_config_with_defaults();
    StateStore state_store(config[CONFIG_KEY_STORE_PATH]);
    library_log().load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "library.log");
    reading_stats::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "stats.log");
    collections::load(std::filesystem::path(config[CONFIG_KEY_STORE_PATH]) / "collections.tsv");
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
        settings_set_font_name(state_store, sys_styling.get_font_name());
        settings_set_font_size(state_store, sys_styling.get_font_size());
        settings_set_shoulder_keymap(state_store, sys_styling.get_shoulder_keymap());
    });

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
    token_view_styling.set_word_spacing(settings_get_small_int(state_store, "word_spacing").value_or(1));
    token_view_styling.set_cover_sleep(settings_get_small_int(state_store, "cover_sleep").value_or(0));
    token_view_styling.set_night_theme(settings_get_night_theme(state_store).value_or(std::string()));
    token_view_styling.set_night_from(settings_get_night_hour(state_store, true).value_or(21));
    token_view_styling.set_night_until(settings_get_night_hour(state_store, false).value_or(7));
    token_view_styling.set_auto_exit_minutes(settings_get_auto_exit_minutes(state_store).value_or(0));
    book_layout::attach(token_view_styling);
    if (const auto speed = settings_get_reading_speed(state_store))
    {
        reading_speed::set_units_per_second(*speed);
    }
    reading_speed::set_saver([&state_store](double speed) {
        settings_set_reading_speed(state_store, speed);
    });
    token_view_styling.subscribe_to_changes([&token_view_styling, &state_store]() {
        // Persist changes
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
            settings_set_small_int(state_store, "word_spacing", token_view_styling.get_word_spacing());
            settings_set_small_int(state_store, "cover_sleep", token_view_styling.get_cover_sleep());
            settings_set_night_theme(state_store, token_view_styling.get_night_theme());
            settings_set_night_hour(state_store, true, token_view_styling.get_night_from());
            settings_set_night_hour(state_store, false, token_view_styling.get_night_until());
            settings_set_indent_level(state_store, token_view_styling.get_indent_level());
            settings_set_paragraph_gap(state_store, token_view_styling.get_paragraph_gap());
            settings_set_bold_text(state_store, token_view_styling.get_bold_text());
        }
        settings_set_auto_exit_minutes(state_store, token_view_styling.get_auto_exit_minutes());
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
            SW_BTN_R2
        }
    );
    SystemKeyChordTracker chord_tracker;

    TriggerInput trigger_input;
    auto is_trigger = [](SDLKey key) {
        return key == SW_BTN_L2 || key == SW_BTN_R2;
    };
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
    SDL_BlitSurface(screen, NULL, video, NULL);
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
                        ran_user_code = handle_key_down(key) || ran_user_code;
                    }
                    break;
                case SDL_KEYUP:
                    {
                        SDLKey key = event.key.keysym.sym;
                        if (trigger_input.active() && is_trigger(key))
                        {
                            break;
                        }
                        chord_tracker.on_keyrelease(key);
                    }
                    break;
                default:
                    break;
            }
        }

        quit = quit || chord_tracker.exit_requested();

        held_key_tracker.accumulate(avg_loop_time); // Pretend perfect loop timing for event firing consistency
        ran_user_code = held_key_tracker.for_newest_held(key_held_callback) || ran_user_code;
        ran_user_code = view_stack.tick(avg_loop_time) || ran_user_code;

        if (ran_user_code)
        {
            bool force_render = view_stack.pop_completed_views();

            if (view_stack.is_done())
            {
                quit = true;
            }

            if (view_stack.render(screen, force_render))
            {
                SDL_BlitSurface(screen, NULL, video, NULL);
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
                        SDL_BlitSurface(screen, NULL, video, NULL);
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
    state_store.flush();
    reading_speed::flush();
    library_log().flush();
    reading_stats::flush();

    SDL_FreeSurface(screen);
    SDL_Quit();
    xmlCleanupParser();
    
    return 0;
}
