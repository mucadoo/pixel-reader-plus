#include "./battery_estimate.h"

#include <deque>

namespace battery_estimate
{

namespace
{

struct Point
{
    long long when;
    int percent;
    bool exact; // seen to change; the first sample is only where watching began
};

std::deque<Point> points; // the moments the charge changed, oldest first
bool charging_now = false;

constexpr size_t MAX_POINTS = 16;
constexpr int MIN_DROP = 1;
constexpr long long MIN_SECONDS = 10 * 60;
// Points older than this say nothing about how the battery is being used now.
constexpr long long MAX_AGE = 3 * 60 * 60;

} // namespace

void reset()
{
    points.clear();
    charging_now = false;
}

void note(long long now, int percent, bool charging)
{
    charging_now = charging;
    if (percent < 0)
    {
        return;
    }
    if (charging)
    {
        points.clear();
        return;
    }
    if (!points.empty() && percent > points.back().percent)
    {
        points.clear(); // charged in between
    }
    if (!points.empty() && now - points.back().when > MAX_AGE)
    {
        points.clear();
    }
    if (points.empty() || percent < points.back().percent)
    {
        points.push_back({now, percent, !points.empty()});
        while (points.size() > MAX_POINTS)
        {
            points.pop_front();
        }
    }
}

std::optional<long long> seconds_left(long long now)
{
    if (charging_now || points.size() < 3)
    {
        return std::nullopt;
    }
    size_t from = 0;
    while (from < points.size() && !points[from].exact)
    {
        ++from;
    }
    if (from + 1 >= points.size())
    {
        return std::nullopt;
    }
    const Point &first = points[from];
    const Point &last = points.back();
    const int drop = first.percent - last.percent;
    const long long seconds = last.when - first.when;
    if (drop < MIN_DROP || seconds < MIN_SECONDS)
    {
        return std::nullopt;
    }
    // seconds for each percent, then the percent left, less the time since the last drop
    const double per_percent = static_cast<double>(seconds) / drop;
    double left = per_percent * last.percent - static_cast<double>(now - last.when);
    if (left < 0)
    {
        left = 0;
    }
    return static_cast<long long>(left);
}

std::string format(long long seconds)
{
    const long long minutes = seconds / 60;
    if (minutes < 60)
    {
        return std::to_string(minutes < 1 ? 1 : minutes) + "m";
    }
    // to the nearest ten minutes
    const long long rounded = (minutes + 5) / 10 * 10;
    return std::to_string(rounded / 60) + "h " + (rounded % 60 < 10 ? "0" : "") + std::to_string(rounded % 60) + "m";
}

} // namespace battery_estimate
