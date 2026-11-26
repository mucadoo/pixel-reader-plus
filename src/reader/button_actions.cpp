#include "./button_actions.h"

namespace button_actions
{

int get(Slot slot)
{
    switch (slot)
    {
        case SLOT_START: return ACTION_BOOKMARK;
        case SLOT_A: return ACTION_TITLE_BAR;
        default: return ACTION_NOTHING;
    }
}

} // namespace button_actions