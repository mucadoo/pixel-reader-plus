#include "./bookmarks.h"

#include <algorithm>
#include <fstream>
#include <sstream>

std::vector<Bookmark> load_bookmarks(const std::filesystem::path &path)
{
    std::vector<Bookmark> bookmarks;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream fields(line);
        DocAddr address = 0;
        if (!(fields >> address))
        {
            continue;
        }
        std::string label;
        std::getline(fields, label);
        if (!label.empty() && label.front() == '\t')
        {
            label.erase(0, 1);
        }
        bookmarks.push_back({address, label});
    }
    std::stable_sort(bookmarks.begin(), bookmarks.end(), [](const Bookmark &a, const Bookmark &b) {
        return a.address < b.address;
    });
    return bookmarks;
}

void save_bookmarks(const std::filesystem::path &path, const std::vector<Bookmark> &bookmarks)
{
    std::ofstream out(path, std::ios::trunc);
    for (const Bookmark &bookmark : bookmarks)
    {
        out << bookmark.address << '\t' << bookmark.label << '\n';
    }
}

bool bookmark_in_range(const std::vector<Bookmark> &bookmarks, DocAddr begin, DocAddr end)
{
    for (const Bookmark &bookmark : bookmarks)
    {
        if (bookmark.address >= begin && bookmark.address < end)
        {
            return true;
        }
    }
    return false;
}