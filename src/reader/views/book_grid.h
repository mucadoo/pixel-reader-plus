#ifndef BOOK_GRID_H_
#define BOOK_GRID_H_

#include "reader/views/menu_draw.h"
#include "util/throttled.h"

#include <SDL/SDL_keysym.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_video.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class BookCovers;
class SystemStyling;

// The book list as a grid of covers. A book without a cover gets a plain colored
// cover with its title, and a folder gets a folder tile. The chosen cell's name
// slides when it is too long for its cell.
class BookGrid
{
public:
    struct Item
    {
        enum class Kind { Up, Folder, Book };

        Kind kind = Kind::Book;
        std::string label;
        std::string tag; // file type for a book, such as EPUB
        int progress = -1; // percent read, or -1 for a book never opened
        bool finished = false;
        std::string badge; // a short label for the corner of the cover, such as Done or To read
        std::filesystem::path path;
    };

private:
    struct Layout
    {
        int line_height = 0;
        int caption_height = 0;
        int cols = 1;
        int rows = 1;
        int cell_w = 0;
        int cell_h = 0;
        int cover_w = 0;
        int cover_h = 0;
        int origin_x = 0;
        int origin_y = 0;
    };

    SystemStyling &styling;
    const uint32_t styling_sub_id;
    BookCovers *covers = nullptr;

    std::vector<Item> items;
    std::string title;
    std::vector<std::string> hints;
    std::string empty_message;

    uint32_t cursor_pos = 0;
    uint32_t first_row = 0;
    bool needs_render = true;
    menu_draw::Marquee marquee;
    Throttled move_throttle;

    std::function<void(uint32_t)> on_focus;
    std::function<void(uint32_t)> on_open;

    Layout compute_layout() const;
    TTF_Font *caption_font() const;
    void ensure_visible();
    void move_to(uint32_t index);
    void draw_cell(SDL_Surface *dest, const Layout &layout, uint32_t index, int cell_x, int cell_y);
    void draw_placeholder(SDL_Surface *dest, const Item &item, int x, int y, int w, int h, bool selected);

public:
    // How big the tiles are: 0 large, 1 medium, 2 small. Shared by every grid.
    static void set_density(int level);

    explicit BookGrid(SystemStyling &styling);
    ~BookGrid();
    BookGrid(const BookGrid &) = delete;
    BookGrid &operator=(const BookGrid &) = delete;

    void set_covers(BookCovers *covers);
    void set_items(std::vector<Item> new_items);
    void set_title(std::string new_title);
    void set_hints(std::vector<std::string> new_hints);
    void set_empty_message(std::string message);
    // Move the cursor without telling the focus callback.
    void set_cursor(uint32_t index);
    uint32_t cursor() const;
    void set_on_focus(std::function<void(uint32_t)> callback);
    void set_on_open(std::function<void(uint32_t)> callback);
    void invalidate();

    bool render(SDL_Surface *dest, bool force_render);
    // Slide the long name and pick up covers. True when the grid has to be drawn again.
    bool tick(uint32_t elapsed_ms);
    void on_keypress(SDLKey key);
    void on_keyheld(SDLKey key, uint32_t held_time_ms);
};

#endif
