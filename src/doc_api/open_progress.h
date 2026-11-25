#ifndef OPEN_PROGRESS_H_
#define OPEN_PROGRESS_H_

#include <cstdint>
#include <functional>

// Opening a book for the first time can take a while, because every part of it
// is read once to measure it. Whoever opens the book can ask to hear how far
// that has got, to show it. The hook is only set while a book is opening.
void set_open_progress_hook(std::function<void(uint32_t done, uint32_t total)> hook);
void report_open_progress(uint32_t done, uint32_t total);

#endif
