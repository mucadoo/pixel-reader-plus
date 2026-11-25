#ifndef READING_SPEED_H_
#define READING_SPEED_H_

#include <cstdint>
#include <functional>

// How fast the reader goes through a book, in address units per second, learned
// from the pages actually turned. Used for the time left in the chapter and book.
namespace reading_speed
{

// Units per second. Starts from a guess of a slow-to-average reader.
double units_per_second();
void set_units_per_second(double value);

// Call with the position in the book each time it may have moved.
void note_position(uint32_t position, long long now_seconds);
// Forget where the last page was, e.g. when a book is opened.
void reset_position();

// Called with the new speed when it should be saved.
void set_saver(std::function<void(double)> saver);
void flush();

// Seconds to cover `units`, or -1 when no estimate can be made.
long long seconds_for(uint32_t units);

} // namespace reading_speed

#endif