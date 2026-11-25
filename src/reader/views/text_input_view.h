#ifndef TEXT_INPUT_VIEW_H_
#define TEXT_INPUT_VIEW_H_

#include "reader/view.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SystemStyling;

// An on-screen keyboard for typing a short line of text, such as a search word or a file name.
class TextInputView: public View
{
    bool _is_done = false;
    bool _needs_render = true;

    std::string heading;
    std::string text;
    size_t max_length;
    std::function<void(const std::string &)> on_done;
    int page = 0;
    int cursor_col = 0;
    int cursor_row = 0;
    SystemStyling &styling;
    uint32_t styling_sub_id;

    void type_key();
    void erase_last();

public:
    TextInputView(
        std::string heading,
        std::string initial,
        std::function<void(const std::string &)> on_done,
        SystemStyling &styling,
        size_t max_length = 80
    );
    virtual ~TextInputView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    bool consumes_x() const override { return true; }
};

#endif
