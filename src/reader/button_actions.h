#ifndef BUTTON_ACTIONS_H_
#define BUTTON_ACTIONS_H_

// What the Start button and the A button do while reading, and what holding A, Y or Start does.
// The jobs are fixed: Start toggles a bookmark, A shows or hides the bar, the holds do nothing.
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
    ACTION_NEXT_PROFILE = 12,
    ACTION_NEXT_THEME = 13,
    ACTION_BIGGER_TEXT = 14,
    ACTION_SMALLER_TEXT = 15,
    ACTION_LOOK_UP = 16,
    ACTION_WORD_LIST = 17,
    ACTION_HIGHLIGHTS = 18,
    ACTION_BOOKMARKS = 19,
    ACTION_COUNT = 20,
};

// How long a button is held for the hold action, in milliseconds.
constexpr unsigned HOLD_MILLISECONDS = 700;

// What the slot does.
int get(Slot slot);

} // namespace button_actions

#endif
