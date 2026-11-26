#ifndef GUIDE_VIEW_H_
#define GUIDE_VIEW_H_

#include "reader/view.h"

#include <cstdint>

struct SystemStyling;

// The controls guide: one screen per topic (reading, selecting a word, the book menu, the book
// list, settings), each a list of buttons with what they do. Left and right change topic, up and
// down scroll, A or B close it.
class GuideView: public View
{
    bool _is_done = false;
    bool _needs_render = true;
    uint32_t section = 0;
    uint32_t first_row = 0;
    SystemStyling &styling;
    uint32_t styling_sub_id;

    void change_section(int direction);

public:
    explicit GuideView(SystemStyling &styling);
    virtual ~GuideView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif
