#ifndef BATTERY_H_
#define BATTERY_H_

// The charge of the battery in percent, or -1 when the system does not say.
int battery_percent();

// True while the battery is being charged.
bool battery_charging();

#endif