#include "./book_style.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{

std::string trim(const std::string &s)
{
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
    {
        ++begin;
    }
    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
    {
        --end;
    }
    return s.substr(begin, end - begin);
}

std::string lower_copy(std::string s)
{
    for (char &c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

namespace
{
bool g_book_styles = true;
} // namespace

void set_book_styles_enabled(bool enabled)
{
    g_book_styles = enabled;
}

bool book_styles_enabled()
{
    return g_book_styles;
}

void Stylesheet::parse(const std::string &css)
{
    if (!g_book_styles)
    {
        return;
    }
    std::string text;
    text.reserve(css.size());
    for (size_t i = 0; i < css.size(); ++i)
    {
        if (css[i] == '/' && i + 1 < css.size() && css[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < css.size() && !(css[i] == '*' && css[i + 1] == '/'))
            {
                ++i;
            }
            if (i + 1 < css.size())
            {
                ++i;
            }
            continue;
        }
        text.push_back(css[i]);
    }

    size_t i = 0;
    int order = 0;
    while (i < text.size())
    {
        size_t brace = text.find('{', i);
        if (brace == std::string::npos)
        {
            break;
        }
        std::string prelude = text.substr(i, brace - i);
        size_t end = text.find('}', brace);
        if (end == std::string::npos)
        {
            break;
        }
        std::string body = text.substr(brace + 1, end - brace - 1);
        i = end + 1;
        if (prelude.find('@') != std::string::npos)
        {
            continue;
        }

        Rule base;
        base.order = order++;
        size_t prop = 0;
        while (prop < body.size())
        {
            size_t semi = body.find(';', prop);
            if (semi == std::string::npos)
            {
                semi = body.size();
            }
            std::string decl = body.substr(prop, semi - prop);
            prop = semi + 1;
            size_t colon = decl.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            std::string name = lower_copy(trim(decl.substr(0, colon)));
            std::string value = lower_copy(trim(decl.substr(colon + 1)));
            if (name == "font-size")
            {
                char *stop = nullptr;
                float n = std::strtof(value.c_str(), &stop);
                if (stop == value.c_str())
                {
                    continue;
                }
                std::string unit = trim(std::string(stop));
                if (unit == "pt")
                {
                    base.font_pt = n;
                }
                else if (unit == "em" || unit == "rem")
                {
                    base.font_em = n;
                }
                else if (unit == "%")
                {
                    base.font_pct = n / 100.f;
                }
                else if (unit == "px")
                {
                    base.font_pt = n * 0.75f;
                }
            }
            else if (name == "font-weight")
            {
                if (value == "bold" || value == "bolder" || (value.size() == 3 && value[0] >= '6'))
                {
                    base.bold = 1;
                }
                else if (value == "normal" || value == "400")
                {
                    base.bold = 0;
                }
            }
            else if (name == "font-style")
            {
                if (value == "italic" || value == "oblique")
                {
                    base.italic = 1;
                }
                else if (value == "normal")
                {
                    base.italic = 0;
                }
            }
            else if (name == "page-break-before" || name == "break-before" || name == "page-break-after" || name == "break-after")
            {
                size_t bang = value.find('!');
                if (bang != std::string::npos)
                {
                    value = trim(value.substr(0, bang));
                }
                int forced = -1;
                if (value == "always" || value == "left" || value == "right" || value == "page" || value == "recto" || value == "verso")
                {
                    forced = 1;
                }
                else if (value == "auto" || value == "avoid" || value == "avoid-page")
                {
                    forced = 0;
                }
                if (forced >= 0)
                {
                    if (name.find("after") != std::string::npos)
                    {
                        base.break_after = forced;
                    }
                    else
                    {
                        base.break_before = forced;
                    }
                }
            }
            else if (name == "text-align")
            {
                if (value == "center")
                {
                    base.align = 1;
                }
                else if (value == "left" || value == "justify" || value == "start")
                {
                    base.align = 2;
                }
            }
        }

        size_t sel_pos = 0;
        while (sel_pos < prelude.size())
        {
            size_t comma = prelude.find(',', sel_pos);
            if (comma == std::string::npos)
            {
                comma = prelude.size();
            }
            std::string sel = lower_copy(trim(prelude.substr(sel_pos, comma - sel_pos)));
            sel_pos = comma + 1;
            if (sel.empty() || sel.find(' ') != std::string::npos || sel.find('>') != std::string::npos)
            {
                continue;
            }
            auto pseudo = sel.find(':');
            if (pseudo != std::string::npos)
            {
                sel = sel.substr(0, pseudo);
            }
            Rule rule = base;
            auto dot = sel.find('.');
            if (dot == std::string::npos)
            {
                rule.tag = sel;
            }
            else
            {
                rule.tag = sel.substr(0, dot);
                rule.cls = sel.substr(dot + 1);
                auto hash = rule.cls.find('#');
                if (hash != std::string::npos)
                {
                    rule.cls = rule.cls.substr(0, hash);
                }
            }
            if (rule.tag.empty() && rule.cls.empty())
            {
                continue;
            }
            rule.specificity = (rule.tag.empty() ? 0 : 1) + (rule.cls.empty() ? 0 : 10);
            rules.push_back(std::move(rule));
        }
    }
}

BookStyle Stylesheet::apply(
    const BookStyle &parent,
    const std::string &tag,
    const std::vector<std::string> &classes
) const
{
    BookStyle next = parent;
    // A break belongs to the element that declared it, not to every child.
    next.break_before = 0;
    next.break_before_set = false;
    next.break_after = 0;
    next.break_after_set = false;
    float parent_scale = parent.scale > 0.2f ? parent.scale : 1.f;
    std::string name = lower_copy(tag);

    std::vector<const Rule *> matched;
    for (const Rule &rule : rules)
    {
        if (!rule.tag.empty() && rule.tag != name)
        {
            continue;
        }
        if (!rule.cls.empty())
        {
            bool found = false;
            for (const std::string &cls : classes)
            {
                if (lower_copy(cls) == rule.cls)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                continue;
            }
        }
        matched.push_back(&rule);
    }

    std::sort(matched.begin(), matched.end(), [](const Rule *a, const Rule *b) {
        if (a->specificity != b->specificity)
        {
            return a->specificity < b->specificity;
        }
        return a->order < b->order;
    });

    for (const Rule *rule : matched)
    {
        if (rule->font_pt >= 0)
        {
            next.scale = rule->font_pt / 12.f;
            next.scale_set = true;
        }
        else if (rule->font_em >= 0)
        {
            next.scale = parent_scale * rule->font_em;
            next.scale_set = true;
        }
        else if (rule->font_pct >= 0)
        {
            next.scale = parent_scale * rule->font_pct;
            next.scale_set = true;
        }
        if (rule->bold >= 0)
        {
            next.bold = rule->bold == 1;
        }
        if (rule->italic >= 0)
        {
            next.italic = rule->italic == 1;
        }
        if (rule->break_before >= 0)
        {
            next.break_before = rule->break_before;
            next.break_before_set = true;
        }
        if (rule->break_after >= 0)
        {
            next.break_after = rule->break_after;
            next.break_after_set = true;
        }
        if (rule->align == 1)
        {
            next.center = true;
            next.center_set = true;
        }
        else if (rule->align == 2)
        {
            next.center = false;
            next.center_set = true;
        }
    }

    if (next.scale < 0.65f)
    {
        next.scale = 0.65f;
    }
    if (next.scale > 3.f)
    {
        next.scale = 3.f;
    }
    return next;
}
