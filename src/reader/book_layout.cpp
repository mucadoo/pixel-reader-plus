#include "./book_layout.h"

#include "./font_catalog.h"
#include "./layout_options.h"
#include "./reading_mode.h"
#include "./system_styling.h"
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
    bool ligatures = false;
    bool widow_control = false;
    bool keep_punctuation = false;
    std::string language;
    ReadingMode reading = ReadingMode::Scroll;
    std::string font; // empty when the system styling is not attached
    int font_size = 0;
};

TokenViewStyling *styling = nullptr;
SystemStyling *system_styling = nullptr;
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
    layout.ligatures = styling->get_ligatures();
    layout.widow_control = styling->get_widow_control();
    layout.keep_punctuation = styling->get_keep_punctuation();
    layout.language = styling->get_book_language();
    layout.reading = styling->get_reading_mode();
    if (system_styling)
    {
        layout.font = system_styling->get_font_name();
        layout.font_size = static_cast<int>(system_styling->get_font_size());
    }
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
    styling->set_ligatures(layout.ligatures);
    styling->set_widow_control(layout.widow_control);
    styling->set_keep_punctuation(layout.keep_punctuation);
    styling->set_book_language(layout.language);
    styling->set_reading_mode(layout.reading);
    if (system_styling && !layout.font.empty() && layout.font_size > 0)
    {
        system_styling->set_font_name(get_valid_font_name(layout.font));
        system_styling->set_font_size(static_cast<uint32_t>(layout.font_size));
    }
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
            else if (key == "ligatures") layout.ligatures = value == "1";
            else if (key == "keep_punctuation") layout.keep_punctuation = value == "1";
            else if (key == "widows") layout.widow_control = value == "1";
            else if (key == "language") layout.language = value;
            else if (key == "reading") layout.reading = value == "page" ? ReadingMode::Page : ReadingMode::Scroll;
            else if (key == "font") layout.font = value;
            else if (key == "font_size") layout.font_size = std::stoi(value);
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

void attach_system(SystemStyling &s)
{
    system_styling = &s;
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

void reset()
{
    if (!styling || !is_active)
    {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(book_file, ec);
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
        << "ligatures=" << (layout.ligatures ? 1 : 0) << "\n"
        << "keep_punctuation=" << (layout.keep_punctuation ? 1 : 0) << "\n"
        << "widows=" << (layout.widow_control ? 1 : 0) << "\n"
        << "language=" << layout.language << "\n"
        << "reading=" << (layout.reading == ReadingMode::Page ? "page" : "scroll") << "\n";
    if (!layout.font.empty())
    {
        out << "font=" << layout.font << "\n"
            << "font_size=" << layout.font_size << "\n";
    }
}

} // namespace book_layout
