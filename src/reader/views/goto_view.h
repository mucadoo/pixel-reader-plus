#ifndef GOTO_VIEW_H_
#define GOTO_VIEW_H_

#include "reader/view.h"

#include <cstdint>
#include <functional>
#include <string>

struct SystemStyling;

// A small panel for picking a number in a range, such as a percent or a page.
class GotoView: public View
{
    bool _is_done = false;
    bool _needs_render = true;

    std::string heading;
    std::string suffix;
    uint32_t lo;
    uint32_t hi;
    uint32_t percent;
    std::function<void(uint32_t)> on_choose;
    SystemStyling &styling;
    uint32_t styling_sub_id;

public:
    GotoView(
        std::string heading,
        std::string suffix,
        uint32_t lo,
        uint32_t hi,
        uint32_t start,
        std::function<void(uint32_t)> on_choose,
        SystemStyling &styling
    );
    virtual ~GotoView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif