#include "./work_pump.h"

namespace
{

std::function<bool()> work_step;

} // namespace

void reader_set_work(std::function<bool()> step)
{
    work_step = std::move(step);
}

void reader_clear_work()
{
    work_step = nullptr;
}

bool reader_step_work()
{
    if (!work_step)
    {
        return false;
    }
    return work_step();
}
