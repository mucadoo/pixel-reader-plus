#ifndef BOOK_INFO_VIEW_H_
#define BOOK_INFO_VIEW_H_

#include "reader/view.h"

#include <SDL/SDL_video.h>

#include <string>
#include <utility>
#include <vector>

struct SystemStyling;

// The page about one book: its cover, a list of facts and the description.
// Up and down scroll, any other button closes it.
class BookInfoView: public View
{
public:
    struct Line
    {
        std::string text;
        int x = 0;
        int y = 0; // from the top of the content
        bool quiet = false;
    };

private:
    bool _is_done = false;
    bool _needs_render = true;
    std::string title;
    std::vector<std::pair<std::string, std::string>> facts;
    std::string description;
    SDL_Surface *cover = nullptr; // our own copy
    SystemStyling &styling;
    uint32_t styling_sub_id;

    int scroll_px = 0;
    int content_h = 0;
    int view_h = 0;
    int line_h = 0;

public:
    // The cover is copied, so the caller keeps its own. It may be null.
    BookInfoView(
        std::string title,
        std::vector<std::pair<std::string, std::string>> facts,
        std::string description,
        SDL_Surface *cover,
        SystemStyling &styling
    );
    virtual ~BookInfoView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
};

#endif
