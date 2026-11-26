#include "./button_actions.h"

#include <fstream>
#include <system_error>

namespace button_actions
{

namespace
{

std::filesystem::path actions_file;
int chosen[SLOT_COUNT] = {ACTION_BOOKMARK, ACTION_TITLE_BAR};

const char *const ACTION_NAMES[ACTION_COUNT] = {
    "Toggle a bookmark",
    "Show or hide the bar",
    "Chapters",
    "Search in book",
    "Go to %",
    "Book map",
    "Jump back",
    "Next chapter",
    "Previous chapter",
    "Nothing",
};

void save()
{
    if (actions_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(actions_file.parent_path(), ec);
    std::ofstream out(actions_file, std::ios::trunc);
    for (int slot = 0; slot < SLOT_COUNT; ++slot)
    {
        out << chosen[slot] << '\n';
    }
}

} // namespace

int default_for(Slot slot)
{
    return slot == SLOT_START ? ACTION_BOOKMARK : ACTION_TITLE_BAR;
}

int valid_action(int value, Slot slot)
{
    return value >= 0 && value < ACTION_COUNT ? value : default_for(slot);
}

void load(std::filesystem::path file)
{
    actions_file = std::move(file);
    for (int slot = 0; slot < SLOT_COUNT; ++slot)
    {
        chosen[slot] = default_for(static_cast<Slot>(slot));
    }
    std::ifstream in(actions_file);
    for (int slot = 0; slot < SLOT_COUNT; ++slot)
    {
        int value = -1;
        if (!(in >> value))
        {
            break;
        }
        chosen[slot] = valid_action(value, static_cast<Slot>(slot));
    }
}

int get(Slot slot)
{
    return chosen[slot];
}

void set(Slot slot, int action)
{
    chosen[slot] = valid_action(action, slot);
    save();
}

const char *action_name(int action)
{
    return ACTION_NAMES[valid_action(action, SLOT_START)];
}

const char *slot_name(Slot slot)
{
    return slot == SLOT_START ? "Start button" : "A button";
}

} // namespace button_actions
