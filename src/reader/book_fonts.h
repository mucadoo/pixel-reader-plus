#ifndef BOOK_FONTS_H_
#define BOOK_FONTS_H_

#include <filesystem>
#include <string>

class DocReader;

// The font a book carries inside itself. Its files are copied out of the book into a folder,
// since the font library reads files.
namespace book_fonts
{

// The folder the copies go in.
void set_dir(const std::filesystem::path &dir);

// Takes the faces the reader lists. Does nothing, and leaves no font active, when use is false.
void open(const DocReader &reader, const std::string &book_id, bool use);

// Forgets the font, when a book is closed.
void clear();

bool active();

// The file to draw text of this style with, and in style the flags that file does not
// already have, which the font library must still apply. Needs active().
std::string pick(int &style);

// Part of the page layout key, empty when no font is active.
std::string key();

} // namespace book_fonts

#endif