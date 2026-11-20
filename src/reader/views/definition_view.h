#ifndef DEFINITION_VIEW_H_
#define DEFINITION_VIEW_H_

#include "reader/view.h"

#include <string>

struct SystemStyling;

class DefinitionView: public View
{
    bool _is_done = false;
    bool _needs_render = true;
    uint32_t scroll_pos = 0;
    std::string title;
    std::string body;
    std::string font_name;
    SystemStyling &styling;
    uint32_t styling_sub_id;

public:
    DefinitionView(std::string title, std::string body, SystemStyling &styling);
    virtual ~DefinitionView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif
