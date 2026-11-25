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
