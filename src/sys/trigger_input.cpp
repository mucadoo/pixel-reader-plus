#include "./trigger_input.h"

#include "./keymap.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <linux/input.h>
#include <string>
#include <unistd.h>

namespace
{

const char *PAD_NAME = "MIYOO Player1";

// The pad reports 0 at rest and 255 while a trigger is pressed.
constexpr int PRESS_THRESHOLD = 127;

int open_pad()
{
    DIR *dir = opendir("/sys/class/input");
    if (!dir)
    {
        return -1;
    }

    int fd = -1;
    while (dirent *entry = readdir(dir))
    {
        if (std::strncmp(entry->d_name, "event", 5) != 0)
        {
            continue;
        }
        std::ifstream name_file(std::string("/sys/class/input/") + entry->d_name + "/device/name");
        std::string name;
        std::getline(name_file, name);
        if (name != PAD_NAME)
        {
            continue;
        }
        fd = open((std::string("/dev/input/") + entry->d_name).c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0)
        {
            break;
        }
    }
    closedir(dir);
    return fd;
}

} // namespace

TriggerInput::TriggerInput()
    : fd(open_pad())
{
}

TriggerInput::~TriggerInput()
{
    close_pad();
}

void TriggerInput::close_pad()
{
    if (fd >= 0)
    {
        close(fd);
    }
    fd = -1;
    seen_events = false;
    l2_down = false;
    r2_down = false;
}

bool TriggerInput::active() const
{
    return fd >= 0 && seen_events;
}

bool TriggerInput::is_down(SDLKey key) const
{
    if (key == SW_BTN_L2)
    {
        return l2_down;
    }
    if (key == SW_BTN_R2)
    {
        return r2_down;
    }
    return false;
}

void TriggerInput::poll(const std::function<void(SDLKey, bool)> &on_change)
{
    if (fd < 0)
    {
        return;
    }

    input_event events[32];
    while (true)
    {
        ssize_t n = read(fd, events, sizeof(events));
        if (n < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                // The pad went away. Fall back to the keys gptokeyb sends.
                bool l2_was_down = l2_down;
                bool r2_was_down = r2_down;
                close_pad();
                if (l2_was_down)
                {
                    on_change(SW_BTN_L2, false);
                }
                if (r2_was_down)
                {
                    on_change(SW_BTN_R2, false);
                }
            }
            return;
        }
        if (n == 0)
        {
            return;
        }

        seen_events = true;
        size_t count = static_cast<size_t>(n) / sizeof(input_event);
        for (size_t i = 0; i < count; ++i)
        {
            const input_event &event = events[i];
            if (event.type != EV_ABS)
            {
                continue;
            }

            bool *state = nullptr;
            SDLKey key = SDLK_UNKNOWN;
            if (event.code == ABS_Z)
            {
                state = &l2_down;
                key = SW_BTN_L2;
            }
            else if (event.code == ABS_RZ)
            {
                state = &r2_down;
                key = SW_BTN_R2;
            }
            else
            {
                continue;
            }

            bool down = event.value > PRESS_THRESHOLD;
            if (down != *state)
            {
                *state = down;
                on_change(key, down);
            }
        }
    }
}
