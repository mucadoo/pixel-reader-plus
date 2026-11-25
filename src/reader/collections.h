#ifndef COLLECTIONS_H_
#define COLLECTIONS_H_

#include <filesystem>
#include <string>
#include <vector>

// Named shelves of books, kept apart from the folders the books sit in.
namespace collections
{

// Reads the file if it exists. Changes are written by flush().
void load(std::filesystem::path file);
void flush();

// Every collection, by name.
std::vector<std::string> names();
// The books of a collection that still exist, in the order they were added.
std::vector<std::filesystem::path> books(const std::string &name);
int count(const std::string &name);
bool contains(const std::string &name, const std::filesystem::path &book);

void add(const std::string &name, const std::filesystem::path &book);
void remove(const std::string &name, const std::filesystem::path &book);
// A collection exists until it is deleted, even with no books.
void create(const std::string &name);
void delete_collection(const std::string &name);

// The book was deleted, or renamed to another path.
void forget_book(const std::filesystem::path &book);
void rename_book(const std::filesystem::path &from, const std::filesystem::path &to);

} // namespace collections

#endif
