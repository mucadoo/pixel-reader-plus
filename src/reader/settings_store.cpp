#include "./settings_store.h"
#include "./layout_options.h"
#include "./state_store.h"
#include "./util/string_serialization.h"

#include <iostream>
#include <vector>

namespace
{

constexpr const char *SETTINGS_KEY_SHOW_TITLE_BAR = "show_title_bar";
constexpr const char *SETTINGS_SHOULDER_KEYMAP = "shoulder_keymap";
constexpr const char *SETTINGS_KEY_COLOR_THEME = "color_theme";
constexpr const char *SETTINGS_KEY_FONT_NAME = "font_name";
constexpr const char *SETTINGS_KEY_FONT_SIZE = "font_size";
constexpr const char *SETTINGS_PROGRESS_REPORTING = "progress_reporting";
constexpr const char *SETTINGS_READING_MODE = "reading_mode";
constexpr const char *SETTINGS_BROWSER_VIEW = "browser_view";
constexpr const char *SETTINGS_CLOCK_FORMAT = "clock_format";

const char *bar_slot_key(int slot)
{
    switch (slot)
    {
        case 0:
            return "bar_left";
        case 1:
            return "bar_middle";
        default:
            return "bar_right";
    }
}

} // namespace

std::optional<BarItem> settings_get_bar_item(const StateStore &state_store, int slot)
{
    const auto &opt = state_store.get_setting(bar_slot_key(slot));
    if (!opt)
    {
        return std::nullopt;
    }
    return decode_bar_item(*opt);
}

void settings_set_bar_item(StateStore &state_store, int slot, BarItem item)
{
    state_store.set_setting(bar_slot_key(slot), encode_bar_item(item));
}

std::optional<ClockFormat> settings_get_clock_format(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting(SETTINGS_CLOCK_FORMAT);
    if (!opt)
    {
        return std::nullopt;
    }
    return decode_clock_format(*opt);
}

void settings_set_clock_format(StateStore &state_store, ClockFormat format)
{
    state_store.set_setting(SETTINGS_CLOCK_FORMAT, encode_clock_format(format));
}

namespace
{

std::optional<int> get_level(const StateStore &state_store, const char *key)
{
    const auto &opt = state_store.get_setting(key);
    if (!opt)
    {
        return std::nullopt;
    }
    try
    {
        const int level = std::stoi(*opt);
        if (level >= 0 && level < LAYOUT_LEVEL_COUNT)
        {
            return level;
        }
    }
    catch (...)
    {
    }
    return std::nullopt;
}

} // namespace

std::optional<int> settings_get_margin_level(const StateStore &state_store)
{
    return get_level(state_store, "margin_level");
}

void settings_set_margin_level(StateStore &state_store, int level)
{
    state_store.set_setting("margin_level", std::to_string(level));
}

std::optional<int> settings_get_spacing_level(const StateStore &state_store)
{
    return get_level(state_store, "spacing_level");
}

void settings_set_spacing_level(StateStore &state_store, int level)
{
    state_store.set_setting("spacing_level", std::to_string(level));
}

std::optional<bool> settings_get_justify(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting("justify");
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "on";
}

void settings_set_justify(StateStore &state_store, bool justify)
{
    state_store.set_setting("justify", justify ? "on" : "off");
}

std::optional<BrowserSort> settings_get_browser_sort(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting("browser_sort");
    if (!opt)
    {
        return std::nullopt;
    }
    return decode_browser_sort(*opt);
}

void settings_set_browser_sort(StateStore &state_store, BrowserSort sort)
{
    state_store.set_setting("browser_sort", encode_browser_sort(sort));
}

std::optional<double> settings_get_reading_speed(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting("reading_speed");
    if (!opt)
    {
        return std::nullopt;
    }
    try
    {
        return std::stod(*opt);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void settings_set_reading_speed(StateStore &state_store, double speed)
{
    state_store.set_setting("reading_speed", std::to_string(speed));
}

std::optional<bool> settings_get_hide_finished(const StateStore &state_store)
{
    const auto opt = state_store.get_setting("hide_finished");
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "1";
}

void settings_set_hide_finished(StateStore &state_store, bool hide)
{
    state_store.set_setting("hide_finished", hide ? "1" : "0");
}

std::optional<int> settings_get_auto_exit_minutes(const StateStore &state_store)
{
    const auto opt = state_store.get_setting("auto_exit_minutes");
    if (!opt)
    {
        return std::nullopt;
    }
    try
    {
        return std::stoi(*opt);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void settings_set_auto_exit_minutes(StateStore &state_store, int minutes)
{
    state_store.set_setting("auto_exit_minutes", std::to_string(minutes));
}

std::optional<bool> settings_get_hyphenate(const StateStore &state_store)
{
    const auto opt = state_store.get_setting("hyphenate");
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "1";
}

void settings_set_hyphenate(StateStore &state_store, bool hyphenate)
{
    state_store.set_setting("hyphenate", hyphenate ? "1" : "0");
}

std::optional<bool> settings_get_browser_grid(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting(SETTINGS_BROWSER_VIEW);
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "grid";
}

void settings_set_browser_grid(StateStore &state_store, bool grid)
{
    state_store.set_setting(SETTINGS_BROWSER_VIEW, grid ? "grid" : "list");
}

std::optional<bool> settings_get_show_title_bar(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting(SETTINGS_KEY_SHOW_TITLE_BAR);
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "true";
}

void settings_set_show_title_bar(StateStore &state_store, bool show_title_bar)
{
    state_store.set_setting(SETTINGS_KEY_SHOW_TITLE_BAR, show_title_bar ? "true" : "false");
}

std::optional<std::string> settings_get_shoulder_keymap(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_SHOULDER_KEYMAP);
}

void settings_set_shoulder_keymap(StateStore &state_store, std::string keymap)
{
    state_store.set_setting(SETTINGS_SHOULDER_KEYMAP, keymap);
}

std::optional<ProgressReporting> settings_get_progress_reporting(const StateStore &state_store)
{
    auto progress = state_store.get_setting(SETTINGS_PROGRESS_REPORTING);
    if (!progress)
    {
        return std::nullopt;
    }

    return decode_progress_reporting(*progress);
}

void settings_set_progress_reporting(StateStore &state_store, ProgressReporting progress_reporting)
{
    state_store.set_setting(SETTINGS_PROGRESS_REPORTING, encode_progress_reporting(progress_reporting));
}

std::optional<ReadingMode> settings_get_reading_mode(const StateStore &state_store)
{
    auto mode = state_store.get_setting(SETTINGS_READING_MODE);
    if (!mode)
    {
        return std::nullopt;
    }
    return decode_reading_mode(*mode);
}

void settings_set_reading_mode(StateStore &state_store, ReadingMode reading_mode)
{
    state_store.set_setting(SETTINGS_READING_MODE, encode_reading_mode(reading_mode));
}

std::optional<std::string> settings_get_color_theme(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_COLOR_THEME);
}

void settings_set_color_theme(StateStore &state_store, const std::string &color_theme)
{
    state_store.set_setting(SETTINGS_KEY_COLOR_THEME, color_theme);
}

std::optional<std::string> settings_get_font_name(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_FONT_NAME);
}

void settings_set_font_name(StateStore &state_store, const std::string &font_name)
{
    state_store.set_setting(SETTINGS_KEY_FONT_NAME, font_name);
}

std::optional<uint32_t> settings_get_font_size(const StateStore &state_store)
{
    auto font_size = state_store.get_setting(SETTINGS_KEY_FONT_SIZE);
    if (!font_size)
    {
        return std::nullopt;
    }
    return try_decode_uint(*font_size);
}

void settings_set_font_size(StateStore &state_store, uint32_t font_size)
{
    state_store.set_setting(SETTINGS_KEY_FONT_SIZE, std::to_string(font_size));
}
