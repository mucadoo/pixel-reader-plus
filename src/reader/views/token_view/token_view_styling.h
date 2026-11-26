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
    // The other three margins, each a level of margin_pixels (right) or vertical_margin_pixels (top, bottom)
    int get_margin_right() const;
    void set_margin_right(int level);
    int get_margin_top() const;
    void set_margin_top(int level);
    int get_margin_bottom() const;
    void set_margin_bottom(int level);

    // Reading ruler mode: 0 off, 1 line, 2 focus
    // Size of the tiles in the book grid: 0 large, 1 medium, 2 small
    int get_grid_size() const;
    void set_grid_size(int level);

    // Extra space after each letter: 0 normal, 1 slight, 2 wide, 3 wider
    int get_letter_spacing() const;
    void set_letter_spacing(int level);

    int get_ruler() const;
    void set_ruler(int mode);

    int get_spacing_level() const;
    void set_spacing_level(int level);
    bool get_justify() const;
    void set_justify(bool justify);

    // How pictures in the book are shown, a PictureMode
    int get_picture_mode() const;
    void set_picture_mode(int mode);

    // A thin line along the bottom edge showing how far through the book you are, cut at each chapter
    bool get_thin_bar() const;
    void set_thin_bar(bool on);

    // Lines of the old screen kept when a scroll or page jump moves on (0 to 3)
    int get_scroll_overlap() const;
    void set_scroll_overlap(int lines);

    // Extra space between words: 0 Tight, 1 Normal, 2 Wide, 3 Wider (see layout_options.h)
    int get_word_spacing() const;
    void set_word_spacing(int level);

    // Whether the stylesheets of epub books are used. Takes effect when the next book is opened.
    bool get_book_styles() const;
    void set_book_styles(bool on);

    // How much the whole picture is darkened (see screen_dim.h): 0 Off to 3 Dimmest
    int get_screen_dim() const;
    void set_screen_dim(int level);

    // The dictionary to look words up in: a name from dictionary_names(), or empty to follow the
    // language of the book.
    const std::string &get_dictionary() const;
    void set_dictionary(const std::string &name);

    // Book search: bit 1 whole words only, bit 2 match case. And the last searches, joined by |.
    int get_search_options() const;
    void set_search_options(int bits);
    const std::string &get_search_history() const;
    void set_search_history(const std::string &joined);

    // Color of new highlights: 0 Yellow, 1 Green, 2 Blue, 3 Pink
    int get_highlight_color() const;
    void set_highlight_color(int color);

    // Idle time before the cover is shown: 0 Off, then the steps of cover_sleep_minutes()
    int get_cover_sleep() const;
    void set_cover_sleep(int step);

    // Automatic night theme: the theme to use between two hours of the day. Empty name for off.
    const std::string &get_night_theme() const;
    void set_night_theme(const std::string &name);
    int get_night_from() const;
    void set_night_from(int hour);
    int get_night_until() const;
    void set_night_until(int hour);

    // First-line indent step (see layout_options.h), the blank line between paragraphs, and bold body text
    int get_indent_level() const;
    void set_indent_level(int level);
    bool get_paragraph_gap() const;
    void set_paragraph_gap(bool gap);
    bool get_bold_text() const;
    void set_bold_text(bool bold);

    // Break long words at the end of a line
    bool get_hyphenate() const;
    void set_hyphenate(bool hyphenate);

    // Minutes without a key press after which the reader saves and quits. 0 is never.
    int get_auto_exit_minutes() const;
    void set_auto_exit_minutes(int minutes);

    // Subscribe to any changes
    uint32_t subscribe_to_changes(std::function<void()> callback);
    void unsubscribe_from_changes(uint32_t sub_id);
};

#endif
