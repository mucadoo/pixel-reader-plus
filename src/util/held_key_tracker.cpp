#include "./held_key_tracker.h"

#include <SDL/SDL.h>

HeldKeyTracker::HeldKeyTracker(std::vector<SDLKey> keycodes)
    : keycodes(keycodes),
      held_times(keycodes.size(), 0)
{
}

HeldKeyTracker::~HeldKeyTracker()
{
}

void HeldKeyTracker::set_held_override(std::function<std::optional<bool>(SDLKey)> override_fn)
{
    held_override = std::move(override_fn);
}

void HeldKeyTracker::set_device_key(std::function<SDLKey(SDLKey)> map_fn)
{
    device_key = std::move(map_fn);
}

void HeldKeyTracker::accumulate(uint32_t ms)
{
    const Uint8 *keystate = SDL_GetKeyState(nullptr);

    auto key_it = keycodes.begin();
    auto time_it = held_times.begin();
    while (key_it != keycodes.end())
    {
        std::optional<bool> overridden = held_override ? held_override(*key_it) : std::nullopt;
        bool held = overridden ? *overridden : keystate[device_key ? device_key(*key_it) : *key_it] != 0;
        if (held)
        {
            *time_it += ms;
        }
        else
        {
            *time_it = 0;
        }

        ++key_it;
        ++time_it;
    }
}

bool HeldKeyTracker::for_newest_held(const std::function<void(SDLKey, uint32_t)> &callback)
{
    uint32_t newest_time = 0;
    SDLKey newest_key = SDLK_UNKNOWN;

    auto key_it = keycodes.begin();
    auto time_it = held_times.begin();
    while (key_it != keycodes.end())
    {
        SDLKey key = *key_it;
        uint32_t time = *time_it;
        if (time != 0 && (newest_time == 0 || time < newest_time))
        {
            newest_time = time;
            newest_key = key;
        }

        ++key_it;
        ++time_it;
    }

    if (newest_time != 0)
    {
        callback(newest_key, newest_time);
        return true;
    }

    return false;
}
