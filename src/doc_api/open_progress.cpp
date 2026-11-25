#include "./open_progress.h"

namespace
{

std::function<void(uint32_t, uint32_t)> open_hook;

} // namespace

void set_open_progress_hook(std::function<void(uint32_t done, uint32_t total)> hook)
{
    open_hook = std::move(hook);
}

void report_open_progress(uint32_t done, uint32_t total)
{
    if (open_hook)
    {
        open_hook(done, total);
    }
}
