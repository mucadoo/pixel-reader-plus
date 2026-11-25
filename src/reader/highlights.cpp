#include "./highlights.h"

#include "doc_api/doc_addr.h"

#include <algorithm>
#include <fstream>
#include <sstream>

std::vector<Highlight> load_highlights(const std::filesystem::path &path)
{
    std::vector<Highlight> highlights;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream fields(line);
        DocAddr begin = 0;
        DocAddr end = 0;
        if (!(fields >> begin >> end) || end <= begin)
        {
            continue;
        }
        std::string text;
        std::getline(fields, text);
        if (!text.empty() && text.front() == ' ')
        {
            text.erase(0, 1);
        }
        highlights.emplace_back(begin, end, text);
    }
    return highlights;
}

void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights)
{
    std::ofstream out(path, std::ios::trunc);
    for (const Highlight &highlight : highlights)
    {
        out << highlight.first << ' ' << highlight.second;
        if (!highlight.text.empty())
        {
            out << ' ' << highlight.text;
        }
        out << '\n';
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

void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end, const std::string &text)
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
    highlights.emplace_back(begin, end, text);
}
