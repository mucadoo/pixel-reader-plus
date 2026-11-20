#include "./highlights.h"

#include "doc_api/doc_addr.h"

#include <algorithm>
#include <fstream>

std::vector<Highlight> load_highlights(const std::filesystem::path &path)
{
    std::vector<Highlight> highlights;
    std::ifstream in(path);
    DocAddr begin = 0;
    DocAddr end = 0;
    while (in >> begin >> end)
    {
        if (end > begin)
        {
            highlights.emplace_back(begin, end);
        }
    }
    return highlights;
}

void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights)
{
    std::ofstream out(path, std::ios::trunc);
    for (const Highlight &highlight : highlights)
    {
        out << highlight.first << ' ' << highlight.second << '\n';
    }
}

bool highlights_cover(const std::vector<Highlight> &highlights, DocAddr begin)
{
    for (const Highlight &highlight : highlights)
    {
        if (begin >= highlight.first && begin < highlight.second)
        {
            return true;
        }
    }
    return false;
}

bool highlight_covers_range(const std::vector<Highlight> &highlights, DocAddr begin, DocAddr end)
{
    if (end <= begin)
    {
        return false;
    }
    for (const Highlight &highlight : highlights)
    {
        if (highlight.first <= begin && highlight.second >= end)
        {
            return true;
        }
    }
    return false;
}

void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end)
{
    if (end <= begin)
    {
        return;
    }
    if (highlight_covers_range(highlights, begin, end))
    {
        highlights.erase(
            std::remove_if(
                highlights.begin(),
                highlights.end(),
                [begin, end](const Highlight &highlight) {
                    return highlight.first < end && highlight.second > begin;
                }
            ),
            highlights.end()
        );
        return;
    }
    highlights.emplace_back(begin, end);
}
