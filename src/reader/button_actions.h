#ifndef BUTTON_ACTIONS_H_
#define BUTTON_ACTIONS_H_

#include <filesystem>
#include <string>

// What the Start button and the A button do while reading, and what holding A, Y or Start does.
// Each can be given any of the actions below.
namespace button_actions
{

enum Slot
{
    SLOT_START = 0,
    SLOT_A = 1,
    SLOT_HOLD_A = 2,
    SLOT_HOLD_Y = 3,
    SLOT_HOLD_START = 4,
    SLOT_COUNT = 5,
};

enum Action
{
    ACTION_BOOKMARK = 0,
    ACTION_TITLE_BAR = 1,
    ACTION_CHAPTERS = 2,
    ACTION_SEARCH = 3,
    ACTION_GO_TO_PERCENT = 4,
    ACTION_BOOK_MAP = 5,
    ACTION_JUMP_BACK = 6,
    ACTION_NEXT_CHAPTER = 7,
    ACTION_PREVIOUS_CHAPTER = 8,
    ACTION_NOTHING = 9,
    ACTION_BOOK_MENU = 10,
    ACTION_PICK_WORD = 11,
    ACTION_COUNT = 12,
};

void load(std::filesystem::path file);

int get(Slot slot);
void set(Slot slot, int action);

// How long a button is held for the hold action, in milliseconds.
constexpr unsigned HOLD_MILLISECONDS = 700;

// What the slot does until it is changed.
int default_for(Slot slot);

const char *action_name(int action);
const char *slot_name(Slot slot);

// The action for a number read from a file: anything unknown gives the default for the slot.
int valid_action(int value, Slot slot);

} // namespace button_actions

#endif
