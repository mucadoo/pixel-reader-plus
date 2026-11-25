#ifndef SETTINGS_VIEW_H_
#define SETTINGS_VIEW_H_

#include "reader/view.h"

#include <functional>
#include <string>

struct SystemStyling;
struct TokenViewStyling;

class SettingsView: public View
{
    // The rows, in the order they are listed
    static constexpr uint32_t ROW_BAR_LEFT = 6;
    static constexpr uint32_t ROW_BAR_RIGHT = 8;
    static constexpr uint32_t ROW_CLOCK = 9;
    static constexpr uint32_t ROW_MARGINS = 10;
    static constexpr uint32_t ROW_SPACING = 11;
    static constexpr uint32_t ROW_JUSTIFY = 12;
    static constexpr uint32_t ROW_CONTROLS = 13;
    static constexpr int ROW_COUNT = 14;

    bool _is_done = false;
    bool needs_render = true;
    uint32_t line_selected = 0;
    uint32_t scroll_position = 0;
    std::string font_name;

    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t styling_sub_id;

    int num_menu_items;
    std::function<void()> controls_action;

    void on_change_theme(int dir);
    void on_change_font_size(int dir);
    void on_change_font_name(int dir);
    void on_change_shoulder_keymap(int dir);
    void on_change_progress();
    void on_change_reading();
    void on_change_bar(int slot, int dir);

public:
    SettingsView(
        SystemStyling &sys_styling,
        TokenViewStyling &token_view_styling,
        std::string font_name
    );
    virtual ~SettingsView();

    bool render(SDL_Surface *dest, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;

    void terminate();
    void unterminate();
    void set_controls_action(std::function<void()> action);
};

#endif
