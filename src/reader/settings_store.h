#ifndef SETTINGS_STORE_H_
#define SETTINGS_STORE_H_

#include "./reader/bar_item.h"
#include "./reader/browser_sort.h"
#include "./reader/progress_reporting.h"
#include "./reader/reading_mode.h"

#include <optional>
#include <string>

struct StateStore;

// Title bar
std::optional<bool> settings_get_show_title_bar(const StateStore &state_store);
void settings_set_show_title_bar(StateStore &state_store, bool show_title_bar);

// Shoulder keymap
std::optional<std::string> settings_get_shoulder_keymap(const StateStore &state_store);
void settings_set_shoulder_keymap(StateStore &state_store, std::string keymap);

// Progress reporting
std::optional<ProgressReporting> settings_get_progress_reporting(const StateStore &state_store);
void settings_set_progress_reporting(StateStore &state_store, ProgressReporting progress_reporting);

std::optional<ReadingMode> settings_get_reading_mode(const StateStore &state_store);
void settings_set_reading_mode(StateStore &state_store, ReadingMode reading_mode);

// The three slots of the bar under the text: 0 left, 1 middle, 2 right
std::optional<BarItem> settings_get_bar_item(const StateStore &state_store, int slot);
void settings_set_bar_item(StateStore &state_store, int slot, BarItem item);

std::optional<ClockFormat> settings_get_clock_format(const StateStore &state_store);
void settings_set_clock_format(StateStore &state_store, ClockFormat format);

// Text layout: margin and line spacing steps, and justified lines
std::optional<int> settings_get_margin_level(const StateStore &state_store);
void settings_set_margin_level(StateStore &state_store, int level);
std::optional<int> settings_get_spacing_level(const StateStore &state_store);
void settings_set_spacing_level(StateStore &state_store, int level);
std::optional<bool> settings_get_justify(const StateStore &state_store);
void settings_set_justify(StateStore &state_store, bool justify);

// How fast the reader reads, in address units per second
std::optional<double> settings_get_reading_speed(const StateStore &state_store);
void settings_set_reading_speed(StateStore &state_store, double speed);

// Color theme
std::optional<std::string> settings_get_color_theme(const StateStore &state_store);
void settings_set_color_theme(StateStore &state_store, const std::string &color_theme);

// Font name
std::optional<std::string> settings_get_font_name(const StateStore &state_store);
void settings_set_font_name(StateStore &state_store, const std::string &font_name);

// Book browser view: true for the cover grid, false for the list
std::optional<bool> settings_get_browser_grid(const StateStore &state_store);
void settings_set_browser_grid(StateStore &state_store, bool grid);

// Minutes without a key press after which the reader saves and quits. 0 is never.
std::optional<int> settings_get_auto_exit_minutes(const StateStore &state_store);
void settings_set_auto_exit_minutes(StateStore &state_store, int minutes);

// Hyphenation of words at the end of a line
std::optional<bool> settings_get_hyphenate(const StateStore &state_store);
void settings_set_hyphenate(StateStore &state_store, bool hyphenate);

// Whether finished books are left out of the book list
std::optional<bool> settings_get_hide_finished(const StateStore &state_store);
void settings_set_hide_finished(StateStore &state_store, bool hide);

// How the book list is ordered
std::optional<BrowserSort> settings_get_browser_sort(const StateStore &state_store);
void settings_set_browser_sort(StateStore &state_store, BrowserSort sort);

// Font size
std::optional<uint32_t> settings_get_font_size(const StateStore &state_store);
void settings_set_font_size(StateStore &state_store, uint32_t font_size);

#endif
