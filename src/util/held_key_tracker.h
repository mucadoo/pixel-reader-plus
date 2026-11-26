#ifndef HELD_KEY_TRACKER_H_
#define HELD_KEY_TRACKER_H_

#include <SDL/SDL_keysym.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class HeldKeyTracker {
    std::vector<SDLKey> keycodes;
    std::vector<uint32_t> held_times;
    std::function<std::optional<bool>(SDLKey)> held_override;
    std::function<SDLKey(SDLKey)> device_key;

public:
    HeldKeyTracker(std::vector<SDLKey> keycodes);
    virtual ~HeldKeyTracker();

    // Decides whether a key is held instead of SDL's key state. Return nullopt
    // to use SDL's key state for that key.
    void set_held_override(std::function<std::optional<bool>(SDLKey)> override_fn);

    // Maps a key as the tracker reports it to the key the device reports (for a turned screen).
    void set_device_key(std::function<SDLKey(SDLKey)> map_fn);

    void accumulate(uint32_t ms);
    // Calls back for the key pressed most recently among those held, so an older
    // key that stays down (a drifting stick, for one) cannot take over the repeat.
    bool for_newest_held(const std::function<void(SDLKey, uint32_t)> &callback);
};

#endif
