#ifndef GROUPED_MENU_H_
#define GROUPED_MENU_H_

#include "reader/view.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SystemStyling;

// One row of a menu that is split into themed groups.
struct GroupedEntry
{
    std::string name;
    std::string detail;
    // The group the row is listed under. Empty puts the row on the first screen, outside any group.
    std::string group;
};

// Opens a menu whose first screen has the loose rows, then one row for each group.
// A on a group lists its rows, B goes back to the first screen, and B there closes the menu.
// A group with a single row is not made into a group: the row is shown on the first screen.
// on_choose gets the index into entries; the menu closes after it ran.
// group_order lists the groups in the order to show them; others follow in the order they first appear.
void open_grouped_menu(
    const std::function<void(std::shared_ptr<View>)> &open_view,
    SystemStyling &styling,
    const std::string &title,
    std::vector<GroupedEntry> entries,
    std::function<void(uint32_t)> on_choose,
    std::vector<std::string> group_order = {}
);

#endif