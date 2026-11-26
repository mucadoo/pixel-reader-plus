#include "./layout_profiles.h"

#include "./system_styling.h"
#include "./views/token_view/token_view_styling.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <system_error>

namespace layout_profiles
{

namespace
{

std::filesystem::path profiles_file;
// name -> (key -> value), plus the order of the names
std::vector<std::string> order;
std::map<std::string, std::map<std::string, std::string>> profiles;

std::string clean(std::string name)
{
    for (char &c : name)
    {
        if (c == '\t' || c == '\n' || c == '\r' || c == '=')
        {
            c = ' ';
        }
    }
    while (!name.empty() && name.back() == ' ')
    {
        name.pop_back();
    }
    size_t start = 0;
    while (start < name.size() && name[start] == ' ')
    {
        ++start;
    }
    return name.substr(start);
}

void write_file()
{
    if (profiles_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(profiles_file.parent_path(), ec);
    std::ofstream out(profiles_file, std::ios::trunc);
    for (const std::string &name : order)
    {
        out << name;
        for (const auto &pair : profiles[name])
        {
            out << '\t' << pair.first << '=' << pair.second;
        }
        out << '\n';
    }
}

int number(const std::map<std::string, std::string> &values, const char *key, int fallback)
{
    const auto found = values.find(key);
    return found == values.end() ? fallback : std::atoi(found->second.c_str());
}

} // namespace

void load(std::filesystem::path file)
{
    profiles_file = std::move(file);
    order.clear();
    profiles.clear();
    std::ifstream in(profiles_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        size_t start = 0;
        std::string name;
        std::map<std::string, std::string> values;
        bool first = true;
        while (start <= line.size())
        {
            size_t tab = line.find('\t', start);
            const std::string part = line.substr(start, tab == std::string::npos ? std::string::npos : tab - start);
            if (first)
            {
                name = part;
                first = false;
            }
            else
            {
                const size_t eq = part.find('=');
                if (eq != std::string::npos)
                {
                    values[part.substr(0, eq)] = part.substr(eq + 1);
                }
            }
            if (tab == std::string::npos)
            {
                break;
            }
            start = tab + 1;
        }
        if (!name.empty() && !profiles.count(name))
        {
            order.push_back(name);
            profiles[name] = values;
        }
    }
}

std::vector<std::string> names()
{
    return order;
}

void save(const std::string &raw_name, const SystemStyling &system, const TokenViewStyling &styling)
{
    const std::string name = clean(raw_name);
    if (name.empty())
    {
        return;
    }
    if (!profiles.count(name))
    {
        order.push_back(name);
    }
    std::map<std::string, std::string> values;
    values["font"] = system.get_font_name();
    values["size"] = std::to_string(system.get_font_size());
    values["theme"] = system.get_color_theme();
    values["ml"] = std::to_string(styling.get_margin_level());
    values["mr"] = std::to_string(styling.get_margin_right());
    values["mt"] = std::to_string(styling.get_margin_top());
    values["mb"] = std::to_string(styling.get_margin_bottom());
    values["spacing"] = std::to_string(styling.get_spacing_level());
    values["justify"] = styling.get_justify() ? "1" : "0";
    values["indent"] = std::to_string(styling.get_indent_level());
    values["gap"] = styling.get_paragraph_gap() ? "1" : "0";
    values["bold"] = styling.get_bold_text() ? "1" : "0";
    values["hyphen"] = styling.get_hyphenate() ? "1" : "0";
    values["word"] = std::to_string(styling.get_word_spacing());
    values["letter"] = std::to_string(styling.get_letter_spacing());
    values["pictures"] = std::to_string(styling.get_picture_mode());
    profiles[name] = values;
    write_file();
}

bool apply(const std::string &name, SystemStyling &system, TokenViewStyling &styling)
{
    const auto found = profiles.find(name);
    if (found == profiles.end())
    {
        return false;
    }
    const auto &values = found->second;
    if (const auto font = values.find("font"); font != values.end() && !font->second.empty() && font->second != system.get_font_name())
    {
        system.set_font_name(font->second);
    }
    if (const auto size = values.find("size"); size != values.end())
    {
        const int wanted = std::atoi(size->second.c_str());
        if (wanted > 0 && static_cast<uint32_t>(wanted) != system.get_font_size())
        {
            system.set_font_size(static_cast<uint32_t>(wanted));
        }
    }
    if (const auto theme = values.find("theme"); theme != values.end() && !theme->second.empty() && theme->second != system.get_color_theme())
    {
        system.set_color_theme(theme->second);
    }
    styling.set_margin_level(number(values, "ml", styling.get_margin_level()));
    styling.set_margin_right(number(values, "mr", styling.get_margin_right()));
    styling.set_margin_top(number(values, "mt", styling.get_margin_top()));
    styling.set_margin_bottom(number(values, "mb", styling.get_margin_bottom()));
    styling.set_spacing_level(number(values, "spacing", styling.get_spacing_level()));
    styling.set_justify(number(values, "justify", styling.get_justify() ? 1 : 0) != 0);
    styling.set_indent_level(number(values, "indent", styling.get_indent_level()));
    styling.set_paragraph_gap(number(values, "gap", styling.get_paragraph_gap() ? 1 : 0) != 0);
    styling.set_bold_text(number(values, "bold", styling.get_bold_text() ? 1 : 0) != 0);
    styling.set_hyphenate(number(values, "hyphen", styling.get_hyphenate() ? 1 : 0) != 0);
    styling.set_word_spacing(number(values, "word", styling.get_word_spacing()));
    styling.set_letter_spacing(number(values, "letter", styling.get_letter_spacing()));
    styling.set_picture_mode(number(values, "pictures", styling.get_picture_mode()));
    return true;
}

void remove(const std::string &name)
{
    profiles.erase(name);
    for (auto it = order.begin(); it != order.end(); ++it)
    {
        if (*it == name)
        {
            order.erase(it);
            break;
        }
    }
    write_file();
}

} // namespace layout_profiles