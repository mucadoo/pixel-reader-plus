#ifndef FONT_PREVIEW_VIEW_H_
#define FONT_PREVIEW_VIEW_H_

#include "reader/view.h"

#include <cstdint>
#include <string>

struct SystemStyling;

// Shows a sample paragraph in a font and size before it is chosen.
// Up and down change the font, left and right the size, A keeps the choice and B leaves it as it was.
class FontPreviewView: public View
{
    SystemStyling &styling;
    uint32_t styling_sub_id;
    std::string font_name;
    uint32_t font_size;
    bool _is_done = false;
    bool _needs_render = true;

public:
    explicit FontPreviewView(SystemStyling &styling);
    virtual ~FontPreviewView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif