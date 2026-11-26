#ifndef LIBRARY_STATUS_H_
#define LIBRARY_STATUS_H_

#include <filesystem>
#include <string>

// The short line at the top of the book list: the time, the battery and, when asked for, the free
// space on the card.
namespace library_status
{

// 0 shows nothing, 1 the time and battery, 2 adds the free space.
void configure(int level, bool clock_12_hours);
int level();

// The folder whose card the free space is measured on.
void set_folder(const std::filesystem::path &folder);

// The line, empty when it is off.
std::string text();

} // namespace library_status

#endif