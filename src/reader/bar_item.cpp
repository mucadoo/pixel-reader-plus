#include "./bar_item.h"

namespace
{

struct ItemInfo
{
    BarItem item;
    const char *code;
    const char *label;
};

// In the order the settings step through them.
const ItemInfo ITEMS[] = {
    {BarItem::Empty, "empty", "Nothing"},
    {BarItem::ChapterTitle, "chapter_title", "Chapter title"},
    {BarItem::BookTitle, "book_title", "Book title"},
    {BarItem::Progress, "progress", "Page and %"},
    {BarItem::Page, "page", "Page"},
    {BarItem::ChapterPercent, "chapter_percent", "Chapter %"},
    {BarItem::BookPercent, "book_percent", "Book %"},
    {BarItem::Time, "time", "Time"},
    {BarItem::Date, "date", "Date"},
    {BarItem::ChapterNumber, "chapter_number", "Chapter x of y"},
    {BarItem::ChapterPagesLeft, "chapter_pages_left", "Pages left in chapter"},
    {BarItem::BookPagesLeft, "book_pages_left", "Pages left in book"},
    {BarItem::Battery, "battery", "Battery"},
};
constexpr int ITEM_COUNT = static_cast<int>(sizeof(ITEMS) / sizeof(ITEMS[0]));

int index_of(BarItem item)
{
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        if (ITEMS[i].item == item)
        {
            return i;
        }
    }
    return 0;
}

} // namespace

BarItem get_next_bar_item(BarItem item)
{
    return ITEMS[(index_of(item) + 1) % ITEM_COUNT].item;
}

BarItem get_prev_bar_item(BarItem item)
{
    return ITEMS[(index_of(item) + ITEM_COUNT - 1) % ITEM_COUNT].item;
}

std::optional<BarItem> decode_bar_item(const std::string &item)
{
    for (const auto &info : ITEMS)
    {
        if (item == info.code)
        {
            return info.item;
        }
    }
    // The time-left items are gone; a bar saved with one shows nothing in that place.
    if (item == "chapter_time_left" || item == "book_time_left")
    {
        return BarItem::Empty;
    }
    return std::nullopt;
}

std::string encode_bar_item(BarItem item)
{
    return ITEMS[index_of(item)].code;
}

const char *bar_item_label(BarItem item)
{
    return ITEMS[index_of(item)].label;
}

bool bar_item_is_live(BarItem item)
{
    return item == BarItem::Time || item == BarItem::Date || item == BarItem::Battery;
}

ClockFormat get_next_clock_format(ClockFormat format)
{
    return format == ClockFormat::H24 ? ClockFormat::H12 : ClockFormat::H24;
}

std::optional<ClockFormat> decode_clock_format(const std::string &format)
{
    if (format == "24h")
    {
        return ClockFormat::H24;
    }
    if (format == "12h")
    {
        return ClockFormat::H12;
    }
    return std::nullopt;
}

std::string encode_clock_format(ClockFormat format)
{
    return format == ClockFormat::H12 ? "12h" : "24h";
}

const char *clock_format_label(ClockFormat format)
{
    return format == ClockFormat::H12 ? "12-hour" : "24-hour";
}
