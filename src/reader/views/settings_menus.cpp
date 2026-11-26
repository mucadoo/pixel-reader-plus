#include "./settings_menus.h"

#include "./selection_menu.h"
#include "./text_input_view.h"
#include "filetypes/epub/book_style.h"
#include "reader/layout_profiles.h"
#include "reader/system_styling.h"
#include "reader/view_stack.h"
#include "reader/views/token_view/token_view_styling.h"

#include <memory>
#include <string>
#include <vector>

namespace
{

struct Tweak
{
    int bit;
    const char *name;
};

const Tweak TWEAKS[] = {
    {TWEAK_PLAIN_BREAKS, "Ignore page breaks"},
    {TWEAK_PLAIN_SIZES, "Ignore font sizes"},
    {TWEAK_PLAIN_CENTER, "Ignore centered text"},
    {TWEAK_SMART_QUOTES, "Curly quotes"},
    {TWEAK_PLAIN_FONTS, "Ignore the book's font"},
    {TWEAK_PLAIN_PAGES, "Ignore the book's page numbers"},
};

std::vector<std::string> tweak_rows(int mask)
{
    std::vector<std::string> rows;
    for (const Tweak &tweak : TWEAKS)
    {
        rows.push_back(std::string(tweak.name));
    }
    (void)mask;
    return rows;
}

std::vector<std::string> tweak_details(int mask)
{
    std::vector<std::string> details;
    for (const Tweak &tweak : TWEAKS)
    {
        details.push_back((mask & tweak.bit) ? "On" : "Off");
    }
    return details;
}

} // namespace

void open_style_tweaks_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling)
{
    auto menu = std::make_shared<SelectionMenu>(tweak_rows(styling.get_tweaks()), system);
    menu->set_details(tweak_details(styling.get_tweaks()));
    menu->set_title("Style tweaks (from the next book you open)");
    menu->set_compact(true);
    menu->set_hints({"A  Switch", "B  Close"});
    std::weak_ptr<SelectionMenu> weak = menu;
    menu->set_on_selection([weak, &styling](uint32_t index) {
        if (index >= sizeof(TWEAKS) / sizeof(TWEAKS[0]))
        {
            return;
        }
        styling.set_tweaks(styling.get_tweaks() ^ TWEAKS[index].bit);
        if (auto self = weak.lock())
        {
            self->set_details(tweak_details(styling.get_tweaks()));
        }
    });
    stack.push(menu);
}

namespace
{

void open_profile_actions(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling, const std::string &name)
{
    auto menu = std::make_shared<SelectionMenu>(
        std::vector<std::string> {"Use this layout", "Replace it with the current settings", "Delete it"},
        system
    );
    menu->set_title(name);
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&stack, &system, &styling, name](uint32_t index) {
        if (index == 0)
        {
            layout_profiles::apply(name, system, styling);
        }
        else if (index == 1)
        {
            layout_profiles::save(name, system, styling);
        }
        else if (index == 2)
        {
            layout_profiles::remove(name);
        }
    });
    stack.push(menu);
}

} // namespace

void open_layout_profiles_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling)
{
    std::vector<std::string> rows = {"Save the current settings as..."};
    const std::vector<std::string> saved = layout_profiles::names();
    for (const std::string &name : saved)
    {
        rows.push_back(name);
    }
    auto menu = std::make_shared<SelectionMenu>(rows, system);
    menu->set_title("Layout profiles");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&stack, &system, &styling, saved](uint32_t index) {
        if (index == 0)
        {
            stack.push(std::make_shared<TextInputView>(
                "Name of this layout",
                "",
                [&stack, &system, &styling](const std::string &typed) {
                    if (!typed.empty())
                    {
                        layout_profiles::save(typed, system, styling);
                    }
                },
                system,
                30
            ));
        }
        else if (index - 1 < saved.size())
        {
            open_profile_actions(stack, system, styling, saved[index - 1]);
        }
    });
    stack.push(menu);
}