#ifndef GALLERY_VIEW_H_
#define GALLERY_VIEW_H_

#include "doc_api/doc_addr.h"
#include "reader/view.h"
#include "util/sdl_pointer.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class DocReader;
struct SystemStyling;

struct GalleryPicture
{
    DocAddr address;
    std::filesystem::path path;
};

// Every picture in the book, found by reading it through once. Most pictures first-seen order.
std::vector<GalleryPicture> find_pictures(
    const DocReader &reader,
    size_t max_pictures,
    const std::function<void(int percent)> &progress
);

// All the pictures of the book as a grid of small thumbnails.
// A opens one, Y goes to the place in the book where it is.
class GalleryView: public View
{
    std::shared_ptr<DocReader> reader;
    SystemStyling &styling;
    uint32_t styling_sub_id;
    std::vector<GalleryPicture> pictures;
    std::function<void(const std::filesystem::path &)> on_open;
    std::function<void(DocAddr)> on_go;

    int cols = 4;
    int rows = 3;
    int cursor = 0;
    int first_row = 0;
    std::map<int, surface_unique_ptr> thumbnails; // an empty one when the picture could not be read
    bool _is_done = false;
    bool _needs_render = true;

    int visible_first() const;
    int visible_last() const;
    void load_thumbnail(int index);
    void move_cursor(int cols_delta, int rows_delta);

public:
    GalleryView(
        std::shared_ptr<DocReader> reader,
        std::vector<GalleryPicture> pictures,
        std::function<void(const std::filesystem::path &)> on_open,
        std::function<void(DocAddr)> on_go,
        SystemStyling &styling
    );
    virtual ~GalleryView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    bool tick(uint32_t elapsed_ms) override;
};

#endif