#include "./epub_fonts.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace
{

std::string lower(std::string text)
{
    for (char &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string trim(const std::string &text)
{
    size_t a = 0;
    size_t b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    return text.substr(a, b - a);
}

std::string strip_comments(const std::string &css)
{
    std::string out;
    out.reserve(css.size());
    for (size_t i = 0; i < css.size(); ++i)
    {
        if (css[i] == '/' && i + 1 < css.size() && css[i + 1] == '*')
        {
            const size_t end = css.find("*/", i + 2);
            if (end == std::string::npos)
            {
                break;
            }
            i = end + 1;
            continue;
        }
        out.push_back(css[i]);
    }
    return out;
}

// "Name" or 'Name' or Name, up to the first comma.
std::string first_family(const std::string &value)
{
    std::string v = trim(value);
    std::string out;
    bool quoted = false;
    char quote = 0;
    for (char c : v)
    {
        if (quoted)
        {
            if (c == quote)
            {
                quoted = false;
                continue;
            }
            out.push_back(c);
        }
        else if (c == '"' || c == '\'')
        {
            quoted = true;
            quote = c;
        }
        else if (c == ',')
        {
            break;
        }
        else
        {
            out.push_back(c);
        }
    }
    return lower(trim(out));
}

struct Declarations
{
    std::vector<std::pair<std::string, std::string>> items;

    std::string get(const std::string &name) const
    {
        for (const auto &item : items)
        {
            if (item.first == name)
            {
                return item.second;
            }
        }
        return "";
    }
};

Declarations parse_declarations(const std::string &block)
{
    Declarations out;
    size_t start = 0;
    int parens = 0;
    for (size_t i = 0; i <= block.size(); ++i)
    {
        const char c = i < block.size() ? block[i] : ';';
        if (c == '(')
        {
            ++parens;
        }
        else if (c == ')')
        {
            --parens;
        }
        else if (c == ';' && parens <= 0)
        {
            const std::string decl = block.substr(start, i - start);
            const size_t colon = decl.find(':');
            if (colon != std::string::npos)
            {
                out.items.emplace_back(lower(trim(decl.substr(0, colon))), trim(decl.substr(colon + 1)));
            }
            start = i + 1;
        }
    }
    return out;
}

// The first url(...) that points at a truetype or opentype file.
std::string pick_src(const std::string &value)
{
    size_t pos = 0;
    while ((pos = lower(value).find("url(", pos)) != std::string::npos)
    {
        const size_t close = value.find(')', pos);
        if (close == std::string::npos)
        {
            break;
        }
        std::string url = trim(value.substr(pos + 4, close - pos - 4));
        pos = close + 1;
        if (!url.empty() && (url.front() == '"' || url.front() == '\''))
        {
            url = url.substr(1);
        }
        if (!url.empty() && (url.back() == '"' || url.back() == '\''))
        {
            url.pop_back();
        }
        if (lower(url).rfind("data:", 0) == 0)
        {
            continue;
        }
        const size_t cut = url.find_first_of("?#");
        if (cut != std::string::npos)
        {
            url = url.substr(0, cut);
        }
        const std::string ext = lower(std::filesystem::path(url).extension().string());
        if (ext == ".ttf" || ext == ".otf")
        {
            return url;
        }
    }
    return "";
}

} // namespace

std::vector<FontFace> parse_font_faces(const std::string &css_in, const std::string &css_dir)
{
    const std::string css = strip_comments(css_in);
    const std::string low = lower(css);
    std::vector<FontFace> faces;
    size_t pos = 0;
    while ((pos = low.find("@font-face", pos)) != std::string::npos)
    {
        const size_t open = css.find('{', pos);
        const size_t close = open == std::string::npos ? std::string::npos : css.find('}', open);
        if (close == std::string::npos)
        {
            break;
        }
        pos = close + 1;
        const Declarations decls = parse_declarations(css.substr(open + 1, close - open - 1));
        FontFace face;
        face.family = first_family(decls.get("font-family"));
        const std::string src = pick_src(decls.get("src"));
        if (face.family.empty() || src.empty())
        {
            continue;
        }
        const std::string weight = lower(decls.get("font-weight"));
        if (weight == "bold" || weight == "bolder")
        {
            face.bold = true;
        }
        else if (!weight.empty() && std::isdigit(static_cast<unsigned char>(weight[0])) && weight.find(' ') == std::string::npos)
        {
            face.bold = std::atoi(weight.c_str()) >= 600;
        }
        const std::string style = lower(decls.get("font-style"));
        face.italic = style == "italic" || style == "oblique";
        face.src = (std::filesystem::path(css_dir) / src).lexically_normal().generic_string();
        faces.push_back(face);
    }
    return faces;
}

std::string parse_body_family(const std::string &css_in)
{
    const std::string css = strip_comments(css_in);
    std::string found;
    size_t pos = 0;
    while (pos < css.size())
    {
        const size_t open = css.find('{', pos);
        if (open == std::string::npos)
        {
            break;
        }
        const size_t close = css.find('}', open);
        if (close == std::string::npos)
        {
            break;
        }
        const std::string selectors = lower(css.substr(pos, open - pos));
        pos = close + 1;
        if (selectors.find('@') != std::string::npos)
        {
            continue;
        }
        bool matches = false;
        size_t start = 0;
        for (size_t i = 0; i <= selectors.size(); ++i)
        {
            if (i == selectors.size() || selectors[i] == ',')
            {
                const std::string one = trim(selectors.substr(start, i - start));
                if (one == "body" || one == "html" || one == "p")
                {
                    matches = true;
                }
                start = i + 1;
            }
        }
        if (!matches)
        {
            continue;
        }
        const std::string family = first_family(parse_declarations(css.substr(open + 1, close - open - 1)).get("font-family"));
        if (!family.empty())
        {
            found = family;
        }
    }
    return found;
}