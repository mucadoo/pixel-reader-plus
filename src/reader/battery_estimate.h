#ifndef BATTERY_ESTIMATE_H_
#define BATTERY_ESTIMATE_H_

#include <optional>
#include <string>

// How long the battery will last, worked out from how fast it has been draining: the moments the
// charge dropped by a percent are remembered, and the estimate is the charge left at that pace.
namespace battery_estimate
{

// The charge now (-1 when unknown) and whether it is being charged, at a moment in seconds. Call it now and then.
void note(long long now, int percent, bool charging);

// Seconds left at the pace so far. Nothing until the charge has dropped a few percent over some minutes,
// and while charging.
std::optional<long long> seconds_left(long long now);

// 19800 seconds as "5h 30m", 2700 as "45m".
std::string format(long long seconds);

void reset();

} // namespace battery_estimate

#endif
