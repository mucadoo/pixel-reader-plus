#ifndef TRIGGER_INPUT_H_
#define TRIGGER_INPUT_H_

#include <SDL/SDL_keysym.h>

#include <functional>

// On the Miyoo Flip, L2 and R2 are analog axes on the "MIYOO Player1" pad, and
// gptokeyb does not reliably turn both of them into keys. When that pad is
// present, read the two axes straight from its event device instead.
class TriggerInput
{
    int fd = -1;
    bool seen_events = false;
    bool l2_down = false;
    bool r2_down = false;

    void close_pad();

public:
    TriggerInput();
    ~TriggerInput();
    TriggerInput(const TriggerInput &) = delete;
    TriggerInput &operator=(const TriggerInput &) = delete;

    // True once the pad has delivered events, so L2 and R2 come from here and
    // the copies gptokeyb sends should be ignored.
    bool active() const;
    bool is_down(SDLKey key) const;
    // Report every press and release since the last call.
    void poll(const std::function<void(SDLKey, bool)> &on_change);
};

#endif
