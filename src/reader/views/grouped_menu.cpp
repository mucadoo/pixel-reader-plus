#include "./grouped_menu.h"

#include "./selection_menu.h"
#include "sys/keymap.h"

#include <algorithm>

namespace
{

struct TopRow
{
    std::string name;
    std::string detail;
    int entry = -1;       // the entry the row stands for, or -1 for a group
    std::string group;    // the group the row opens
};

struct Nav
{
    std::string title;
    std::vector<GroupedEntry> entries;
    std::vector<TopRow> top;
    std::vector<int> listed;      // entries of the group on screen, in row order
    std::string open_group;       // empty on the first screen
    size_t open_top_row = 0;      // the row of the first screen that opened the group
    std::function<void(uint32_t)> on_choose;
};

void show_top(Nav &nav, SelectionMenu &menu, size_t cursor)
{
    nav.open_group.clear();
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (const TopRow &row : nav.top)
    {
        names.push_back(row.name);
        details.push_back(row.detail);
    }
    menu.set_entries(names);
    menu.set_details(details);
    menu.set_title(nav.title);
    menu.set_hints({"A  Open", "B  Close"});
    menu.set_cursor_pos(static_cast<uint32_t>(cursor));
}

void show_group(Nav &nav, SelectionMenu &menu, const std::string &group)
{
    nav.open_group = group;
    nav.listed.clear();
    std::vector<std::string> names;
    std::vector<std::string> details;
    for (size_t i = 0; i < nav.entries.size(); ++i)
    {
        if (nav.entries[i].group == group)
        {
            nav.listed.push_back(static_cast<int>(i));
            names.push_back(nav.entries[i].name);
            details.push_back(nav.entries[i].detail);
        }
    }
    menu.set_entries(names);
    menu.set_details(details);
    menu.set_title(nav.title + ": " + group);
    menu.set_hints({"A  Choose", "B  Back"});
}

} // namespace

void open_grouped_menu(
    const std::function<void(std::shared_ptr<View>)> &open_view,
    SystemStyling &styling,
    const std::string &title,
    std::vector<GroupedEntry> entries,
    std::function<void(uint32_t)> on_choose,
    std::vector<std::string> group_order,
    SDLKey close_key
)
{
    auto nav = std::make_shared<Nav>();
    nav->title = title;
    nav->entries = std::move(entries);
    nav->on_choose = std::move(on_choose);

    // How many rows each group has, and the order the groups first appear in.
    std::vector<std::string> groups = group_order;
    for (const GroupedEntry &entry : nav->entries)
    {
        if (!entry.group.empty() && std::find(groups.begin(), groups.end(), entry.group) == groups.end())
        {
            groups.push_back(entry.group);
        }
    }
    const auto size_of = [&nav](const std::string &group) {
        return std::count_if(nav->entries.begin(), nav->entries.end(), [&group](const GroupedEntry &e) { return e.group == group; });
    };
    // Loose rows, then rows that stand alone in their group, then the groups.
    for (size_t i = 0; i < nav->entries.size(); ++i)
    {
        const GroupedEntry &entry = nav->entries[i];
        if (entry.group.empty())
        {
            nav->top.push_back({entry.name, entry.detail, static_cast<int>(i), ""});
        }
    }
    for (const std::string &group : groups)
    {
        const auto count = size_of(group);
        if (count == 1)
        {
            for (size_t i = 0; i < nav->entries.size(); ++i)
            {
                if (nav->entries[i].group == group)
                {
                    nav->top.push_back({nav->entries[i].name, nav->entries[i].detail, static_cast<int>(i), ""});
                }
            }
        }
        else if (count > 1)
        {
            nav->top.push_back({group, ">", -1, group});
        }
    }

    auto menu = std::make_shared<SelectionMenu>(std::vector<std::string> {}, styling);
    menu->set_compact(true);
    menu->set_wide(true);
    std::weak_ptr<SelectionMenu> weak = menu;
    menu->set_on_selection([nav, weak](uint32_t index) {
        auto menu = weak.lock();
        if (!menu)
        {
            return;
        }
        if (nav->open_group.empty())
        {
            if (index >= nav->top.size())
            {
                return;
            }
            const TopRow &row = nav->top[index];
            if (row.entry < 0)
            {
                nav->open_top_row = index;
                const std::string group = row.group;
                show_group(*nav, *menu, group);
                return;
            }
            nav->on_choose(static_cast<uint32_t>(row.entry));
            menu->close();
            return;
        }
        if (index < nav->listed.size())
        {
            nav->on_choose(static_cast<uint32_t>(nav->listed[index]));
            menu->close();
        }
    });
    menu->set_default_on_keypress([close_key](SDLKey key, SelectionMenu &m) {
        if (key == close_key)
        {
            m.close(); // the key that opens the menu also closes it
        }
    });
    menu->set_on_back([nav, weak]() {
        auto menu = weak.lock();
        if (!menu || nav->open_group.empty())
        {
            return false;
        }
        show_top(*nav, *menu, nav->open_top_row);
        return true;
    });
    show_top(*nav, *menu, 0);
    open_view(menu);
}