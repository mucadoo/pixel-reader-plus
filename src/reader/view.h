#ifndef VIEW_H_
#define VIEW_H_

#include <SDL/SDL_keysym.h>
#include <SDL/SDL_video.h>

class View
{
public:
    // Returns true if rendering was performed.
    virtual bool render(SDL_Surface *dest, bool force_render) = 0;

    // Return true if the view is no longer needed.
    virtual bool is_done() = 0;

    // If true, the stack will always re-render the view behind this view.
    virtual bool is_modal() { return false; }

    // Key down event.
    virtual void on_keypress(SDLKey key) = 0;

    // Key up event, for the view on top.
    virtual void on_keyrelease(SDLKey) {}

    // Pass key and held time in ms.
    virtual void on_keyheld(SDLKey, uint32_t) {}

    // Called every frame on the top view with the time since the last frame, for
    // animation and small background jobs. Return true if it needs a redraw.
    virtual bool tick(uint32_t) { return false; }

    // This view has been popped from the stack (now defunct).
    virtual void on_pop() {}

    // True when the view uses the X button itself, so Settings should not open on it.
    virtual bool consumes_x() const { return false; }

    // When the view is now on top of the stack.
    virtual void on_focus() {}

    // Set by a view that ended by doing what it was opened for (a place was chosen, a word was
    // typed), as opposed to being backed out of with B. The menu that opened it closes too.
    bool completed() const { return _completed; }
    void mark_completed() { _completed = true; }

protected:
    bool _completed = false;
};

#endif
