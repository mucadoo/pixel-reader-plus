#ifndef PAGE_BROWSER_VIEW_H_
#define PAGE_BROWSER_VIEW_H_

#include "doc_api/doc_addr.h"
#include "reader/view.h"
#include "util/sdl_pointer.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class DocReader;
struct SystemStyling;

// Skim the book as a grid of tiles. Each tile shows the start of the text at that place, or for a
// comic book a small picture of the page. Choose a tile to go there.
class PageBrowserView: public View
{
    std::shared_ptr<DocReader> reader;
    SystemStyling &styling;
    uint32_t styling_sub_id;
    std::function<void(DocAddr)> on_go;

    bool comic = false;
    int tile_count = 0;
    int cols = 3;
    int rows = 3;
    int cursor = 0;
    int here_tile = 0;
    int first_row = 0;
    std::map<int, std::vector<std::string>> snippets; // text of each tile that was read
    std::map<int, surface_unique_ptr> thumbnails;
    std::map<int, bool> no_thumbnail;                 // tiles whose picture could not be made
    bool _is_done = false;
    bool _needs_render = true;

    DocAddr address_of(int tile) const;
    int tile_of(DocAddr address) const;
    int visible_first() const;
    int visible_last() const;
    bool load_one_pending();
    void load_snippet(int tile);
    void load_thumbnail(int tile, int box_w, int box_h);
    void move_cursor(int cols_delta, int rows_delta);
    int tile_box_w = 0;
    int tile_box_h = 0;

public:
    PageBrowserView(
        std::shared_ptr<DocReader> reader,
        DocAddr current,
        std::function<void(DocAddr)> on_go,
        SystemStyling &styling
    );
    virtual ~PageBrowserView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    bool tick(uint32_t elapsed_ms) override;
};

#endif
