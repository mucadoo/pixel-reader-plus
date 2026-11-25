#ifndef BOOK_LAYOUT_H_
#define BOOK_LAYOUT_H_

#include <filesystem>

class TokenViewStyling;

// Margins, line spacing, justify, hyphenate and Page or Scroll can be kept for
// each book. While a book is open, changing them in Settings changes that book
// only. Outside a book they change the defaults every book starts from.
namespace book_layout
{

// Once, with the styling the reader uses.
void attach(TokenViewStyling &styling);

// A book was opened. Its own layout, if it has one, replaces the defaults.
void open(const std::filesystem::path &file);

// The book was closed. The defaults come back.
void close();

// True while a book is open.
bool active();

// The styling changed. Writes this book's layout when a book is open.
void save_if_active();

} // namespace book_layout

#endif
