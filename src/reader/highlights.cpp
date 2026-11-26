#include "./highlights.h"

#include "doc_api/doc_addr.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
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
        // A note follows the words after a tab.
        std::string note;
        if (const size_t tab = text.find('\t'); tab != std::string::npos)
        {
            note = text.substr(tab + 1);
            text.erase(tab);
        }
        // An optional {u}, {s}, {g}, {b} or {p} in front of the text names the style.
        HighlightStyle style = HighlightStyle::Marker;
        if (text.size() >= 3 && text[0] == '{' && text[2] == '}'
            && (text[1] == 'u' || text[1] == 's' || text[1] == 'g' || text[1] == 'b' || text[1] == 'p')
            && (text.size() == 3 || text[3] == ' '))
        {
            style = text[1] == 'u' ? HighlightStyle::Underline
                : text[1] == 's' ? HighlightStyle::Strikeout
                : text[1] == 'g' ? HighlightStyle::MarkerGreen
                : text[1] == 'b' ? HighlightStyle::MarkerBlue : HighlightStyle::MarkerPink;
            text.erase(0, text.size() == 3 ? 3 : 4);
        }
        // An optional @ and the digits of the time made, after the style.
        long long made = 0;
        if (text.size() >= 10 && text[0] == '@')
        {
            size_t digits = 1;
            while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9')
            {
                ++digits;
            }
            if (digits >= 10 && (digits == text.size() || text[digits] == ' '))
            {
                made = std::atoll(text.substr(1, digits - 1).c_str());
                text.erase(0, digits == text.size() ? digits : digits + 1);
            }
        }
        highlights.emplace_back(begin, end, text, style);
        highlights.back().note = note;
        highlights.back().made = made;
    }
    return highlights;
}

void save_highlights(const std::filesystem::path &path, const std::vector<Highlight> &highlights)
{
    std::ofstream out(path, std::ios::trunc);
    for (const Highlight &highlight : highlights)
    {
        out << highlight.first << ' ' << highlight.second;
        if (highlight.style == HighlightStyle::Underline)
        {
            out << " {u}";
        }
        else if (highlight.style == HighlightStyle::Strikeout)
        {
            out << " {s}";
        }
        else if (highlight.style == HighlightStyle::MarkerGreen)
        {
            out << " {g}";
        }
        else if (highlight.style == HighlightStyle::MarkerBlue)
        {
            out << " {b}";
        }
        else if (highlight.style == HighlightStyle::MarkerPink)
        {
            out << " {p}";
        }
        if (highlight.made > 0)
        {
            out << " @" << highlight.made;
        }
        if (!highlight.text.empty())
        {
            out << ' ' << highlight.text;
        }
        if (!highlight.note.empty())
        {
            std::string note = highlight.note;
            for (char &c : note)
            {
                if (c == '\t' || c == '\n' || c == '\r')
                {
                    c = ' ';
                }
            }
            out << '\t' << note;
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

int highlight_marker_at(const std::vector<Highlight> &highlights, DocAddr begin)
{
    for (const Highlight &highlight : highlights)
    {
        const int color = marker_color_of(highlight.style);
        if (color >= 0 && begin >= highlight.first && begin < highlight.second)
        {
            return color;
        }
    }
    return -1;
}

bool highlights_cover_style(const std::vector<Highlight> &highlights, DocAddr begin, HighlightStyle style)
{
    for (const Highlight &highlight : highlights)
    {
        if (highlight.style == style && begin >= highlight.first && begin < highlight.second)
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

void toggle_highlight(std::vector<Highlight> &highlights, DocAddr begin, DocAddr end, const std::string &text, HighlightStyle style)
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
    highlights.emplace_back(begin, end, text, style);
    highlights.back().made = static_cast<long long>(std::time(nullptr));
}

char highlight_style_letter(HighlightStyle style)
{
    switch (style)
    {
        case HighlightStyle::Underline: return 'u';
        case HighlightStyle::Strikeout: return 's';
        case HighlightStyle::MarkerGreen: return 'g';
        case HighlightStyle::MarkerBlue: return 'b';
        case HighlightStyle::MarkerPink: return 'p';
        default: return 'y';
    }
}

HighlightStyle highlight_style_from_letter(char letter)
{
    switch (letter)
    {
        case 'u': return HighlightStyle::Underline;
        case 's': return HighlightStyle::Strikeout;
        case 'g': return HighlightStyle::MarkerGreen;
        case 'b': return HighlightStyle::MarkerBlue;
        case 'p': return HighlightStyle::MarkerPink;
        default: return HighlightStyle::Marker;
    }
}
