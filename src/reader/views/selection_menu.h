#ifndef SELECTION_MENU_H_
#define SELECTION_MENU_H_

#include "reader/view.h"
#include "reader/views/menu_draw.h"
#include "util/throttled.h"

#include <SDL/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

struct SystemStyling;

class SelectionMenu: public View
{
    bool needs_render = true;

    std::vector<std::string> entries;
    std::string title;
    std::vector<std::string> details;
    std::string empty_message;
    std::vector<std::string> hints = {"A  Open", "B  Close"};
    uint32_t cursor_pos = 0;
    uint32_t scroll_pos = 0;
    int marker_pos = -1;
    menu_draw::Marquee marquee;
    bool close_on_select = false;
    bool compact = false;
    bool wide = false;
    bool x_is_used = false;
    bool show_status = false;
    std::string status_shown;
    uint32_t status_elapsed_ms = 0;

    SystemStyling &styling;
    const uint32_t styling_sub_id;

    const int line_padding = 4;
    int line_height;
    uint32_t num_display_lines() const;
    uint32_t excess_pxl_y() const;

    Throttled scroll_throttle;

    bool _is_done = false;
    std::function<void(uint32_t)> on_selection;
    std::function<void(uint32_t)> on_focus;
    std::function<bool()> on_back;
    std::function<void(SDLKey, SelectionMenu&)> default_on_keypress;

    void on_move_down(uint32_t step);
    void on_move_up(uint32_t step);
    void on_select_entry();

public:

    SelectionMenu(SystemStyling &styling);
    SelectionMenu(std::vector<std::string> entries, SystemStyling &styling);
    virtual ~SelectionMenu();

    void set_entries(std::vector<std::string> new_entries);
    void set_on_selection(std::function<void(uint32_t)> callback);
    void set_on_focus(std::function<void(uint32_t)> callback);
    // Define fallback keypress handler
    void set_default_on_keypress(std::function<void(SDLKey, SelectionMenu &)> callback);
    void set_close_on_select();
    // Shows the library status line (time, battery, free space) before the counter in the title bar.
    void set_show_status() { show_status = true; }
    // B calls this first. When it returns true the press is handled and the menu stays open.
    void set_on_back(std::function<bool()> callback) { on_back = std::move(callback); }
    // The menu uses the X button itself, for example to delete a row.
    void set_uses_x(bool uses) { x_is_used = uses; }
    bool consumes_x() const override { return x_is_used; }
    void set_title(std::string title);
    // A short menu drawn over the screen behind it.
    void set_compact(bool compact);
    // A compact menu drawn like the settings: the full width, with rules under the title and above
    // the hints, which are the first and last of the hints, and the details at the right of the rows.
    void set_wide(bool on) { wide = on; needs_render = true; }
    // Row that stays marked after the cursor moves, such as the chapter being read.
    void set_marker(uint32_t index);
    // Quiet text at the right end of each row, parallel to the entries. Empty means none.
    void set_details(std::vector<std::string> new_details);
    // Shown in the list area when there is nothing to pick. Lines are split on newlines.
    void set_empty_message(std::string message);
    // Button hints along the bottom. The first sits left, the last right, the rest between.
    void set_hints(std::vector<std::string> new_hints);

    void set_cursor_pos(const std::string &entry);
    void set_cursor_pos(uint32_t pos);
    uint32_t get_cursor_pos() const { return cursor_pos; }

    void close();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    bool tick(uint32_t elapsed_ms) override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
};

#endif
