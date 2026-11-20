#ifndef WORK_PUMP_H_
#define WORK_PUMP_H_

#include <functional>

// The open book registers a step that counts pages between frames.
// The step returns true while the main loop should keep running.
void reader_set_work(std::function<bool()> step);
void reader_clear_work();
bool reader_step_work();

#endif
