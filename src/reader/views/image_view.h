#ifndef IMAGE_VIEW_H_
#define IMAGE_VIEW_H_

#include "reader/view.h"
#include "util/sdl_pointer.h"
#include "util/throttled.h"

#include <cstdint>
#include <string>
#include <vector>

struct SystemStyling;

// A picture from the book, full screen, that can be zoomed and moved around.
class ImageView: public View
{
    surface_unique_ptr original;
    surface_unique_ptr scaled; // the picture at the current zoom, empty while it is shown as loaded
    SystemStyling &styling;
    uint32_t styling_sub_id;

    double fit = 1.0;   // scale that makes the whole picture fit the screen
    int zoom_step = 0;
    int offset_x = 0;   // top left of the screen within the zoomed picture
    int offset_y = 0;
    int hint_ms_left = 3500;
    bool _is_done = false;
    bool _needs_render = true;
    Throttled pan_throttle;

    SDL_Surface *current() const;
    bool set_zoom(int step);
    void clamp_offsets();
    void pan(SDLKey key);

public:
    ImageView(const std::vector<char> &data, const std::string &extension, SystemStyling &styling);
    virtual ~ImageView();

    // False when the data could not be read as a picture.
    bool valid() const { return original != nullptr; }

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
    bool tick(uint32_t elapsed_ms) override;
};

#endif
