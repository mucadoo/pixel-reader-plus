#ifndef TOKEN_VIEW_STYLING_H_
#define TOKEN_VIEW_STYLING_H_

#include "reader/bar_item.h"
#include "reader/progress_reporting.h"
#include "reader/reading_mode.h"

#include <functional>
#include <memory>
#include <string>

struct TokenViewStylingState;

class TokenViewStyling
{
    std::unique_ptr<TokenViewStylingState> state;
    void notify_subscribers() const;

public:
    TokenViewStyling(bool show_title_bar, ProgressReporting progress_reporting, ReadingMode reading_mode);
    virtual ~TokenViewStyling();

    // Title bar
    bool get_show_title_bar() const;
    void set_show_title_bar(bool show_title_bar);

    // Progress reporting
    ProgressReporting get_progress_reporting() const;
    void set_progress_reporting(ProgressReporting progress_reporting);

    ReadingMode get_reading_mode() const;
    void set_reading_mode(ReadingMode reading_mode);

    // The three slots of the bar under the text, and how the clock reads there
    BarItem get_bar_left() const;
    BarItem get_bar_middle() const;
    BarItem get_bar_right() const;
    void set_bar_left(BarItem item);
    void set_bar_middle(BarItem item);
    void set_bar_right(BarItem item);
    ClockFormat get_clock_format() const;
    void set_clock_format(ClockFormat format);

    // Margin and line spacing steps (see layout_options.h), and whether lines are justified
    int get_margin_level() const;
    void set_margin_level(int level);
    int get_spacing_level() const;
    void set_spacing_level(int level);
    bool get_justify() const;
    void set_justify(bool justify);

    // Subscribe to any changes
    uint32_t subscribe_to_changes(std::function<void()> callback);
    void unsubscribe_from_changes(uint32_t sub_id);
};

#endif
