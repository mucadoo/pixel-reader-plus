#ifndef BROWSER_SORT_H_
#define BROWSER_SORT_H_

#include <optional>
#include <string>

// How the book list orders the books in a folder. Folders always come first.
enum class BrowserSort {
    Name,
    Title,
    Author,
    Series,
    Recent
};

inline BrowserSort next_browser_sort(BrowserSort sort)
{
    switch (sort)
    {
        case BrowserSort::Name: return BrowserSort::Title;
        case BrowserSort::Title: return BrowserSort::Author;
        case BrowserSort::Author: return BrowserSort::Series;
        case BrowserSort::Series: return BrowserSort::Recent;
        case BrowserSort::Recent: return BrowserSort::Name;
    }
    return BrowserSort::Name;
}

inline std::string encode_browser_sort(BrowserSort sort)
{
    switch (sort)
    {
        case BrowserSort::Title: return "title";
        case BrowserSort::Author: return "author";
        case BrowserSort::Series: return "series";
        case BrowserSort::Recent: return "recent";
        case BrowserSort::Name: break;
    }
    return "name";
}

inline std::optional<BrowserSort> decode_browser_sort(const std::string &text)
{
    if (text == "name") return BrowserSort::Name;
    if (text == "title") return BrowserSort::Title;
    if (text == "author") return BrowserSort::Author;
    if (text == "series") return BrowserSort::Series;
    if (text == "recent") return BrowserSort::Recent;
    return std::nullopt;
}

// Shown next to the folder name. Empty for the default order.
inline const char *browser_sort_suffix(BrowserSort sort)
{
    switch (sort)
    {
        case BrowserSort::Title: return "by title";
        case BrowserSort::Author: return "by author";
        case BrowserSort::Series: return "by series";
        case BrowserSort::Recent: return "recent first";
        case BrowserSort::Name: break;
    }
    return "";
}

#endif