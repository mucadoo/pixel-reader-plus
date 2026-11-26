#ifndef SETTINGS_VIEW_H_
#define SETTINGS_VIEW_H_

#include "reader/view.h"

#include <functional>
#include <map>
#include <string>

struct SystemStyling;
struct TokenViewStyling;

class SettingsView: public View
{
    // The rows, in the order they are listed
    static constexpr uint32_t ROW_BAR_LEFT = 6;
    static constexpr uint32_t ROW_BAR_RIGHT = 8;
    static constexpr uint32_t ROW_CLOCK = 9;
    static constexpr uint32_t ROW_ROTATION = 10;
    static constexpr uint32_t ROW_MARGINS = 11;
    static constexpr uint32_t ROW_MARGIN_RIGHT = 12;
    static constexpr uint32_t ROW_MARGIN_TOP = 13;
    static constexpr uint32_t ROW_MARGIN_BOTTOM = 14;
    static constexpr uint32_t ROW_SPACING = 15;
    static constexpr uint32_t ROW_JUSTIFY = 16;
    static constexpr uint32_t ROW_INDENT = 17;
    static constexpr uint32_t ROW_GAP = 18;
    static constexpr uint32_t ROW_BOLD = 19;
    static constexpr uint32_t ROW_HYPHEN = 20;
    static constexpr uint32_t ROW_PICTURES = 21;
    static constexpr uint32_t ROW_THIN_BAR = 22;
    static constexpr uint32_t ROW_WORD_SPACING = 23;
    static constexpr uint32_t ROW_LETTER_SPACING = 24;
    static constexpr uint32_t ROW_OVERLAP = 25;
    static constexpr uint32_t ROW_RULER = 26;
    static constexpr uint32_t ROW_DICTIONARY = 27;
    static constexpr uint32_t ROW_SCREEN_DIM = 28;
    static constexpr uint32_t ROW_BOOK_STYLES = 29;
    static constexpr uint32_t ROW_HILITE = 30;
    static constexpr uint32_t ROW_NIGHT = 31;
    static constexpr uint32_t ROW_NIGHT_FROM = 32;
    static constexpr uint32_t ROW_NIGHT_UNTIL = 33;
    static constexpr uint32_t ROW_AUTO_EXIT = 34;
    static constexpr uint32_t ROW_COVER_SLEEP = 35;
    static constexpr uint32_t ROW_GRID = 36;
    static constexpr uint32_t ROW_TWEAKS = 37;
    static constexpr uint32_t ROW_PROFILES = 38;
    static constexpr uint32_t ROW_CONTROLS = 39;
    static constexpr int ROW_COUNT = 40;

    bool _is_done = false;
    bool needs_render = true;
    uint32_t line_selected = 0;
    uint32_t scroll_position = 0;
    std::string font_name;

    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t styling_sub_id;

    int num_menu_items;
    std::map<uint32_t, std::function<void()>> row_actions; // rows that open something on A

    void on_change_theme(int dir);
    void on_change_font_size(int dir);
    void on_change_font_name(int dir);
    void on_change_shoulder_keymap(int dir);
    void on_change_progress();
    void on_change_reading();
    void on_change_bar(int slot, int dir);

public:
    static constexpr uint32_t TWEAKS_ROW = ROW_TWEAKS;
    static constexpr uint32_t PROFILES_ROW = ROW_PROFILES;

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
    // Makes A on a row (such as ROW_TWEAKS) run the action.
    void set_row_action(uint32_t row, std::function<void()> action);
};

#endif
