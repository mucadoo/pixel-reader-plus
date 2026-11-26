#ifndef READING_HISTORY_VIEW_H_
#define READING_HISTORY_VIEW_H_

#include "reader/view.h"

#include <SDL/SDL_video.h>

#include <ctime>
#include <functional>
#include <string>

struct SystemStyling;

// A month of reading as a calendar. Each day shows a bar as tall as the time read that day, and the chosen
// day lists the books it was spent on. Left and right move a day, up and down a week, L1 and R1 a month.
class ReadingHistoryView: public View
{
    bool _is_done = false;
    bool _needs_render = true;
    SystemStyling &styling;
    uint32_t styling_sub_id;
    std::function<std::string(const std::string &)> title_for; // a book's path to the title shown
    int year = 1970;
    int month = 0; // 0 to 11
    int day = 1;
    int line_h = 0;

    void shift_days(int delta);
    void shift_months(int delta);

public:
    ReadingHistoryView(SystemStyling &styling, std::function<std::string(const std::string &)> title_for);
    virtual ~ReadingHistoryView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
};

#endif