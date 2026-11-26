#include "./token_view_styling.h"

#include "reader/layout_options.h"

#include <algorithm>
#include <unordered_map>

struct TokenViewStylingState
{
    bool show_title_bar;
    ProgressReporting progress_reporting;
    ReadingMode reading_mode;
    BarItem bar_left = BarItem::ChapterTitle;
    BarItem bar_middle = BarItem::Empty;
    BarItem bar_right = BarItem::Progress;
    ClockFormat clock_format = ClockFormat::H24;
    int margin_level = DEFAULT_MARGIN_LEVEL;
    int spacing_level = DEFAULT_SPACING_LEVEL;
    bool justify = false;
    bool hyphenate = false;
    int indent_level = DEFAULT_INDENT_LEVEL;
    bool paragraph_gap = true;
    std::string night_theme;
    std::string dictionary;
    std::string search_history;
    int search_options = 1;
    bool thin_bar = false;
    int scroll_overlap = 0;
    int margin_right = 0;
    int margin_top = 0;
    int margin_bottom = 0;
    int ruler = 0;
    int letter_spacing = 0;
    int rotation = 0;
    int tweaks = 0;
    int grid_size = 0;
    int word_spacing = 1;
    int cover_sleep = 0;
    int highlight_color = 0;
    int screen_dim = 0;
    bool book_styles = true;
    int picture_mode = 0;
    int night_from = 21;
    int night_until = 7;
    bool bold_text = false;
    bool ligatures = false;
    bool widow_control = false;
    std::string book_language;
    int auto_exit_minutes = 0;

    uint32_t next_subscriber_id = 1;
    std::unordered_map<uint32_t, std::function<void()>> subscribers;

    TokenViewStylingState(bool show_title_bar, ProgressReporting progress_reporting, ReadingMode reading_mode)
        : show_title_bar(show_title_bar), progress_reporting(progress_reporting), reading_mode(reading_mode)
    {}
};

TokenViewStyling::TokenViewStyling(bool show_title_bar, ProgressReporting progress_reporting, ReadingMode reading_mode)
    : state(std::make_unique<TokenViewStylingState>(show_title_bar, progress_reporting, reading_mode))
{
}

TokenViewStyling::~TokenViewStyling()
{
}

void TokenViewStyling::notify_subscribers() const
{
    for (auto &sub: state->subscribers)
    {
        sub.second();
    }
}

bool TokenViewStyling::get_show_title_bar() const
{
    return state->show_title_bar;
}

void TokenViewStyling::set_show_title_bar(bool show_title_bar)
{
    if (state->show_title_bar != show_title_bar)
    {
        state->show_title_bar = show_title_bar;
        notify_subscribers();
    }
}

ProgressReporting TokenViewStyling::get_progress_reporting() const
{
    return state->progress_reporting;
}

void TokenViewStyling::set_progress_reporting(ProgressReporting progress_reporting)
{
    if (state->progress_reporting != progress_reporting)
    {
        state->progress_reporting = progress_reporting;
        notify_subscribers();
    }
}

ReadingMode TokenViewStyling::get_reading_mode() const
{
    return state->reading_mode;
}

void TokenViewStyling::set_reading_mode(ReadingMode reading_mode)
{
    if (state->reading_mode != reading_mode)
    {
        state->reading_mode = reading_mode;
        notify_subscribers();
    }
}

BarItem TokenViewStyling::get_bar_left() const
{
    return state->bar_left;
}

BarItem TokenViewStyling::get_bar_middle() const
{
    return state->bar_middle;
}

BarItem TokenViewStyling::get_bar_right() const
{
    return state->bar_right;
}

void TokenViewStyling::set_bar_left(BarItem item)
{
    if (state->bar_left != item)
    {
        state->bar_left = item;
        notify_subscribers();
    }
}

void TokenViewStyling::set_bar_middle(BarItem item)
{
    if (state->bar_middle != item)
    {
        state->bar_middle = item;
        notify_subscribers();
    }
}

void TokenViewStyling::set_bar_right(BarItem item)
{
    if (state->bar_right != item)
    {
        state->bar_right = item;
        notify_subscribers();
    }
}

ClockFormat TokenViewStyling::get_clock_format() const
{
    return state->clock_format;
}

void TokenViewStyling::set_clock_format(ClockFormat format)
{
    if (state->clock_format != format)
    {
        state->clock_format = format;
        notify_subscribers();
    }
}

int TokenViewStyling::get_margin_level() const
{
    return state->margin_level;
}

void TokenViewStyling::set_margin_level(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->margin_level != level)
    {
        state->margin_level = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_margin_right() const
{
    return state->margin_right;
}

void TokenViewStyling::set_margin_right(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->margin_right != level)
    {
        state->margin_right = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_margin_top() const
{
    return state->margin_top;
}

void TokenViewStyling::set_margin_top(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->margin_top != level)
    {
        state->margin_top = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_margin_bottom() const
{
    return state->margin_bottom;
}

void TokenViewStyling::set_margin_bottom(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->margin_bottom != level)
    {
        state->margin_bottom = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_grid_size() const
{
    return state->grid_size;
}

void TokenViewStyling::set_grid_size(int level)
{
    level = std::clamp(level, 0, 2);
    if (state->grid_size != level)
    {
        state->grid_size = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_letter_spacing() const
{
    return state->letter_spacing;
}

void TokenViewStyling::set_letter_spacing(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->letter_spacing != level)
    {
        state->letter_spacing = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_rotation() const
{
    return state->rotation;
}

void TokenViewStyling::set_rotation(int rotation)
{
    rotation = std::clamp(rotation, 0, 2);
    if (state->rotation != rotation)
    {
        state->rotation = rotation;
        notify_subscribers();
    }
}

int TokenViewStyling::get_tweaks() const
{
    return state->tweaks;
}

void TokenViewStyling::set_tweaks(int mask)
{
    mask &= 0xFF;
    if (state->tweaks != mask)
    {
        state->tweaks = mask;
        notify_subscribers();
    }
}

int TokenViewStyling::get_ruler() const
{
    return state->ruler;
}

void TokenViewStyling::set_ruler(int mode)
{
    mode = std::clamp(mode, 0, 2);
    if (state->ruler != mode)
    {
        state->ruler = mode;
        notify_subscribers();
    }
}

int TokenViewStyling::get_spacing_level() const
{
    return state->spacing_level;
}

void TokenViewStyling::set_spacing_level(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->spacing_level != level)
    {
        state->spacing_level = level;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_justify() const
{
    return state->justify;
}

void TokenViewStyling::set_justify(bool justify)
{
    if (state->justify != justify)
    {
        state->justify = justify;
        notify_subscribers();
    }
}

int TokenViewStyling::get_scroll_overlap() const
{
    return state->scroll_overlap;
}

void TokenViewStyling::set_scroll_overlap(int lines)
{
    lines = std::clamp(lines, 0, 3);
    if (state->scroll_overlap != lines)
    {
        state->scroll_overlap = lines;
        notify_subscribers();
    }
}

int TokenViewStyling::get_word_spacing() const
{
    return state->word_spacing;
}

void TokenViewStyling::set_word_spacing(int level)
{
    level = std::clamp(level, 0, 3);
    if (state->word_spacing != level)
    {
        state->word_spacing = level;
        notify_subscribers();
    }
}

int TokenViewStyling::get_screen_dim() const
{
    return state->screen_dim;
}

void TokenViewStyling::set_screen_dim(int level)
{
    level = std::clamp(level, 0, 3);
    if (state->screen_dim != level)
    {
        state->screen_dim = level;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_book_styles() const
{
    return state->book_styles;
}

void TokenViewStyling::set_book_styles(bool on)
{
    if (state->book_styles != on)
    {
        state->book_styles = on;
        notify_subscribers();
    }
}

int TokenViewStyling::get_highlight_color() const
{
    return state->highlight_color;
}

void TokenViewStyling::set_highlight_color(int color)
{
    color = std::clamp(color, 0, 3);
    if (state->highlight_color != color)
    {
        state->highlight_color = color;
        notify_subscribers();
    }
}

int TokenViewStyling::get_cover_sleep() const
{
    return state->cover_sleep;
}

void TokenViewStyling::set_cover_sleep(int step)
{
    step = std::clamp(step, 0, 3);
    if (state->cover_sleep != step)
    {
        state->cover_sleep = step;
        notify_subscribers();
    }
}

int TokenViewStyling::get_picture_mode() const
{
    return state->picture_mode;
}

void TokenViewStyling::set_picture_mode(int mode)
{
    mode = std::clamp(mode, 0, 3);
    if (state->picture_mode != mode)
    {
        state->picture_mode = mode;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_thin_bar() const
{
    return state->thin_bar;
}

void TokenViewStyling::set_thin_bar(bool on)
{
    if (state->thin_bar != on)
    {
        state->thin_bar = on;
        notify_subscribers();
    }
}

int TokenViewStyling::get_search_options() const
{
    return state->search_options;
}

void TokenViewStyling::set_search_options(int bits)
{
    bits = std::clamp(bits, 0, 3);
    if (state->search_options != bits)
    {
        state->search_options = bits;
        notify_subscribers();
    }
}

const std::string &TokenViewStyling::get_search_history() const
{
    return state->search_history;
}

void TokenViewStyling::set_search_history(const std::string &joined)
{
    if (state->search_history != joined)
    {
        state->search_history = joined;
        notify_subscribers();
    }
}

const std::string &TokenViewStyling::get_dictionary() const
{
    return state->dictionary;
}

void TokenViewStyling::set_dictionary(const std::string &name)
{
    if (state->dictionary != name)
    {
        state->dictionary = name;
        notify_subscribers();
    }
}

const std::string &TokenViewStyling::get_night_theme() const
{
    return state->night_theme;
}

void TokenViewStyling::set_night_theme(const std::string &name)
{
    if (state->night_theme != name)
    {
        state->night_theme = name;
        notify_subscribers();
    }
}

int TokenViewStyling::get_night_from() const
{
    return state->night_from;
}

void TokenViewStyling::set_night_from(int hour)
{
    hour = ((hour % 24) + 24) % 24;
    if (state->night_from != hour)
    {
        state->night_from = hour;
        notify_subscribers();
    }
}

int TokenViewStyling::get_night_until() const
{
    return state->night_until;
}

void TokenViewStyling::set_night_until(int hour)
{
    hour = ((hour % 24) + 24) % 24;
    if (state->night_until != hour)
    {
        state->night_until = hour;
        notify_subscribers();
    }
}

int TokenViewStyling::get_indent_level() const
{
    return state->indent_level;
}

void TokenViewStyling::set_indent_level(int level)
{
    level = std::clamp(level, 0, LAYOUT_LEVEL_COUNT - 1);
    if (state->indent_level != level)
    {
        state->indent_level = level;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_paragraph_gap() const
{
    return state->paragraph_gap;
}

void TokenViewStyling::set_paragraph_gap(bool gap)
{
    if (state->paragraph_gap != gap)
    {
        state->paragraph_gap = gap;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_ligatures() const
{
    return state->ligatures;
}

void TokenViewStyling::set_ligatures(bool on)
{
    if (state->ligatures != on)
    {
        state->ligatures = on;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_widow_control() const
{
    return state->widow_control;
}

void TokenViewStyling::set_widow_control(bool on)
{
    if (state->widow_control != on)
    {
        state->widow_control = on;
        notify_subscribers();
    }
}

const std::string &TokenViewStyling::get_book_language() const
{
    return state->book_language;
}

void TokenViewStyling::set_book_language(const std::string &name)
{
    if (state->book_language != name)
    {
        state->book_language = name;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_bold_text() const
{
    return state->bold_text;
}

void TokenViewStyling::set_bold_text(bool bold)
{
    if (state->bold_text != bold)
    {
        state->bold_text = bold;
        notify_subscribers();
    }
}

bool TokenViewStyling::get_hyphenate() const
{
    return state->hyphenate;
}

void TokenViewStyling::set_hyphenate(bool hyphenate)
{
    if (state->hyphenate != hyphenate)
    {
        state->hyphenate = hyphenate;
        notify_subscribers();
    }
}

int TokenViewStyling::get_auto_exit_minutes() const
{
    return state->auto_exit_minutes;
}

void TokenViewStyling::set_auto_exit_minutes(int minutes)
{
    minutes = std::max(0, minutes);
    if (state->auto_exit_minutes != minutes)
    {
        state->auto_exit_minutes = minutes;
        notify_subscribers();
    }
}

uint32_t TokenViewStyling::subscribe_to_changes(std::function<void()> callback)
{
    uint32_t sub_id = state->next_subscriber_id++;
    state->subscribers[sub_id] = callback;
    return sub_id;
}

void TokenViewStyling::unsubscribe_from_changes(uint32_t sub_id)
{
    state->subscribers.erase(sub_id);
}
