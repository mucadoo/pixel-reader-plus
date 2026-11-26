#include "./book_layout.h"

#include "./layout_options.h"
#include "./reading_mode.h"
#include "./views/token_view/token_view_styling.h"

#include <fstream>
#include <map>
#include <string>
#include <system_error>

namespace book_layout
{

namespace
{

struct Layout
{
    int margin = DEFAULT_MARGIN_LEVEL;
    int margin_right = DEFAULT_MARGIN_LEVEL;
    int margin_top = 0;
    int margin_bottom = 0;
    int spacing = DEFAULT_SPACING_LEVEL;
    bool justify = false;
    bool hyphenate = false;
    int indent = DEFAULT_INDENT_LEVEL;
    bool gap = true;
    bool bold = false;
    ReadingMode reading = ReadingMode::Scroll;
};

TokenViewStyling *styling = nullptr;
bool is_active = false;
bool applying = false;
std::filesystem::path book_file;
Layout defaults;

Layout current()
{
    Layout layout;
    layout.margin = styling->get_margin_level();
    layout.margin_right = styling->get_margin_right();
    layout.margin_top = styling->get_margin_top();
    layout.margin_bottom = styling->get_margin_bottom();
    layout.spacing = styling->get_spacing_level();
    layout.justify = styling->get_justify();
    layout.hyphenate = styling->get_hyphenate();
    layout.indent = styling->get_indent_level();
    layout.gap = styling->get_paragraph_gap();
    layout.bold = styling->get_bold_text();
    layout.reading = styling->get_reading_mode();
    return layout;
}

void apply(const Layout &layout)
{
    applying = true;
    styling->set_margin_level(layout.margin);
    styling->set_margin_right(layout.margin_right);
    styling->set_margin_top(layout.margin_top);
    styling->set_margin_bottom(layout.margin_bottom);
    styling->set_spacing_level(layout.spacing);
    styling->set_justify(layout.justify);
    styling->set_hyphenate(layout.hyphenate);
    styling->set_indent_level(layout.indent);
    styling->set_paragraph_gap(layout.gap);
    styling->set_bold_text(layout.bold);
    styling->set_reading_mode(layout.reading);
    applying = false;
}

bool read_file(const std::filesystem::path &file, Layout &layout)
{
    std::ifstream in(file);
    if (!in)
    {
        return false;
    }
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        try
        {
            if (key == "margin") { layout.margin = std::stoi(value); layout.margin_right = layout.margin; }
            else if (key == "margin_right") layout.margin_right = std::stoi(value);
            else if (key == "margin_top") layout.margin_top = std::stoi(value);
            else if (key == "margin_bottom") layout.margin_bottom = std::stoi(value);
            else if (key == "spacing") layout.spacing = std::stoi(value);
            else if (key == "justify") layout.justify = value == "1";
            else if (key == "hyphenate") layout.hyphenate = value == "1";
            else if (key == "indent") layout.indent = std::stoi(value);
            else if (key == "gap") layout.gap = value == "1";
            else if (key == "bold") layout.bold = value == "1";
            else if (key == "reading") layout.reading = value == "page" ? ReadingMode::Page : ReadingMode::Scroll;
        }
        catch (...)
        {
        }
    }
    return true;
}

} // namespace

void attach(TokenViewStyling &s)
{
    styling = &s;
}

void open(const std::filesystem::path &file)
{
    if (!styling)
    {
        return;
    }
    defaults = current();
    book_file = file;
    is_active = true;
    Layout layout = defaults;
    if (read_file(file, layout))
    {
        apply(layout);
    }
}

void close()
{
    if (!styling || !is_active)
    {
        return;
    }
    is_active = false;
    apply(defaults);
}

bool active()
{
    return is_active;
}

void save_if_active()
{
    if (!styling || !is_active || applying)
    {
        return;
    }
    const Layout layout = current();
    std::error_code ec;
    std::filesystem::create_directories(book_file.parent_path(), ec);
    std::ofstream out(book_file, std::ios::trunc);
    out << "margin=" << layout.margin << "\n"
        << "margin_right=" << layout.margin_right << "\n"
        << "margin_top=" << layout.margin_top << "\n"
        << "margin_bottom=" << layout.margin_bottom << "\n"
        << "spacing=" << layout.spacing << "\n"
        << "justify=" << (layout.justify ? 1 : 0) << "\n"
        << "hyphenate=" << (layout.hyphenate ? 1 : 0) << "\n"
        << "indent=" << layout.indent << "\n"
        << "gap=" << (layout.gap ? 1 : 0) << "\n"
        << "bold=" << (layout.bold ? 1 : 0) << "\n"
        << "reading=" << (layout.reading == ReadingMode::Page ? "page" : "scroll") << "\n";
}

} // namespace book_layout
