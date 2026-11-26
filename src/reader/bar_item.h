#ifndef BAR_ITEM_H_
#define BAR_ITEM_H_

#include <optional>
#include <string>

// What a slot of the bar under the text can show.
enum class BarItem {
    Empty,
    ChapterTitle,
    BookTitle,
    Page,           // page x / y
    ChapterPercent,
    BookPercent,
    Time,
    Date,
    ChapterNumber,    // chapter x of y
    ChapterPagesLeft, // pages left in the chapter
    BookPagesLeft,    // pages left in the book
    BookPageOf,       // the page of the whole book, such as 500 / 600
    ChapterPageOf,    // the page of the chapter, such as 12 / 30
    Battery
};

BarItem get_next_bar_item(BarItem item);
BarItem get_prev_bar_item(BarItem item);
std::optional<BarItem> decode_bar_item(const std::string &item);
std::string encode_bar_item(BarItem item);
// The name shown in the settings.
const char *bar_item_label(BarItem item);
// True for items that change with the time and so need a redraw once a minute.
bool bar_item_is_live(BarItem item);

enum class ClockFormat {
    H24,
    H12
};

ClockFormat get_next_clock_format(ClockFormat format);
std::optional<ClockFormat> decode_clock_format(const std::string &format);
std::string encode_clock_format(ClockFormat format);
const char *clock_format_label(ClockFormat format);

#endif
