#ifndef PENDING_PROGRESS_H_
#define PENDING_PROGRESS_H_

#include <filesystem>
#include <optional>

// A place in a book set by hand from the library, such as for a book read elsewhere. The book
// opens there the next time, and the entry is gone after that.
namespace pending_progress
{

void load(std::filesystem::path file);

// Where the book is to open, as a percent from 0 to 100.
void set(const std::filesystem::path &book, int percent);

// The place set for the book, which is forgotten as it is read.
std::optional<int> take(const std::filesystem::path &book);

} // namespace pending_progress

#endif
