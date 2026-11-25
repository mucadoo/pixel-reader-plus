#ifndef GOTO_VIEW_H_
#define GOTO_VIEW_H_

#include "reader/view.h"

#include <cstdint>
#include <functional>
#include <string>

struct SystemStyling;

// A small panel for picking a place in the book as a percent.
class GotoView: public View
{
    bool _is_done = false;
    bool _needs_render = true;

    uint32_t percent;
    std::function<void(uint32_t)> on_choose;
    SystemStyling &styling;
    uint32_t styling_sub_id;

public:
    GotoView(uint32_t start_percent, std::function<void(uint32_t)> on_choose, SystemStyling &styling);
    virtual ~GotoView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif