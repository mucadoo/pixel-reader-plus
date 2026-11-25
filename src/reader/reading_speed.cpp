#include "./reading_speed.h"

#include <algorithm>

namespace reading_speed
{

namespace
{

constexpr double DEFAULT_SPEED = 25.0;   // about 250 words a minute
constexpr double MIN_SPEED = 3.0;
constexpr double MAX_SPEED = 200.0;
constexpr uint32_t MIN_STEP = 200;       // units to read before taking a sample
constexpr long long MAX_PAUSE = 240;     // a longer wait means the reader left
constexpr uint32_t MAX_JUMP = 20000;     // a bigger move is a jump, not reading

double g_speed = DEFAULT_SPEED;
bool g_has_position = false;
uint32_t g_mark_position = 0;
long long g_mark_time = 0;
bool g_dirty = false;
std::function<void(double)> g_saver;

} // namespace

double units_per_second()
{
    return g_speed;
}

void set_units_per_second(double value)
{
    g_speed = std::clamp(value, MIN_SPEED, MAX_SPEED);
}

void reset_position()
{
    g_has_position = false;
}

void note_position(uint32_t position, long long now_seconds)
{
    if (!g_has_position)
    {
        g_has_position = true;
        g_mark_position = position;
        g_mark_time = now_seconds;
        return;
    }

    if (position < g_mark_position || position - g_mark_position > MAX_JUMP)
    {
        // Went back or jumped: start measuring again from here.
        g_mark_position = position;
        g_mark_time = now_seconds;
        return;
    }

    const uint32_t moved = position - g_mark_position;
    if (moved < MIN_STEP)
    {
        return;
    }

    const long long elapsed = now_seconds - g_mark_time;
    g_mark_position = position;
    g_mark_time = now_seconds;
    if (elapsed < 2 || elapsed > MAX_PAUSE)
    {
        return;
    }

    const double sample = static_cast<double>(moved) / static_cast<double>(elapsed);
    g_speed = std::clamp(g_speed * 0.8 + sample * 0.2, MIN_SPEED, MAX_SPEED);
    g_dirty = true;
}

void set_saver(std::function<void(double)> saver)
{
    g_saver = std::move(saver);
}

void flush()
{
    if (g_dirty && g_saver)
    {
        g_saver(g_speed);
        g_dirty = false;
    }
}

long long seconds_for(uint32_t units)
{
    if (g_speed <= 0.0)
    {
        return -1;
    }
    return static_cast<long long>(static_cast<double>(units) / g_speed);
}

} // namespace reading_speed