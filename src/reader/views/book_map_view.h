#ifndef BOOK_MAP_VIEW_H_
#define BOOK_MAP_VIEW_H_

#include "doc_api/doc_addr.h"
#include "reader/view.h"
#include "util/throttled.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DocReader;
struct SystemStyling;

// The whole book as one bar, with the chapters, bookmarks and highlights marked on it,
// to look over the book and skim to any part of it.
class BookMapView: public View
{
    std::shared_ptr<DocReader> reader;
    SystemStyling &styling;
    uint32_t styling_sub_id;
    std::function<void(DocAddr)> on_go;

    std::vector<double> chapter_marks;   // where each chapter starts, 0 to 1
    std::vector<DocAddr> chapter_addresses;
    std::vector<double> bookmark_marks;
    std::vector<double> highlight_marks;
    double here = 0;                     // where the reader is
    double cursor = 0;                   // where the map points
    int cursor_chapter = -1;             // set when the cursor sits on the start of a chapter
    std::string chapter_name;
    bool _is_done = false;
    bool _needs_render = true;
    Throttled move_throttle;

    double fraction_of(DocAddr address) const;
    void refresh_chapter_name();
    void move_by(double amount);
    void move_to_chapter(int direction);

public:
    BookMapView(
        std::shared_ptr<DocReader> reader,
        DocAddr current,
        const std::vector<DocAddr> &bookmarks,
        const std::vector<DocAddr> &highlights,
        std::function<void(DocAddr)> on_go,
        SystemStyling &styling
    );
    virtual ~BookMapView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
};

#endif
