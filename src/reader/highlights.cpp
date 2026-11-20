#include "./highlights.h"

#include "doc_api/doc_addr.h"

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

void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end)
{
    for (auto it = highlights.begin(); it != highlights.end(); ++it)
    {
        if (it->first == begin)
        {
            highlights.erase(it);
            return;
        }
    }
    highlights.emplace_back(begin, end);
}
