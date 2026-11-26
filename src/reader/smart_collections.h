#ifndef SMART_COLLECTIONS_H_
#define SMART_COLLECTIONS_H_

#include <filesystem>
#include <string>
#include <vector>

// Shelves that fill themselves: a name and conditions, all of which a book must meet to be on it.
namespace smart_collections
{

enum class Field
{
    Author,
    Tag,
    Series,
    Language,
    Publisher,
    Year,
    Status,
    Type,
    Rating,
    COUNT
};

struct Rule
{
    Field field;
    // The text to look for, in any case. A leading '!' turns it round: the book must NOT have it.
    // Year is a year, such as 1984, or a range such as 1980-1989. Rating is a number: at least that many stars.
    // Status is unread, reading, to read, on hold, finished or favorite. Type is epub, txt or md.
    std::string value;
};

// What is known about a book, lowercase not required.
struct Facts
{
    std::string author;
    std::string tags;      // the subjects, separated by "; "
    std::string series;
    std::string language;  // the code and the name, such as "fr French"
    std::string publisher;
    std::string year;      // four digits or empty
    std::string status;    // unread, reading, to read, on hold or finished
    bool favorite = false;
    std::string type;      // epub, txt or md
    int rating = 0;
};

const char *field_name(Field field);
// "Author contains Tolkien", "Status is finished", "Tag does not contain horror".
std::string describe(const Rule &rule);
std::string describe(const std::vector<Rule> &rules);

// True when the book meets every rule. A list of no rules matches nothing.
bool matches(const std::vector<Rule> &rules, const Facts &facts);

// Reads the file if it exists. Changes are written by flush().
void load(std::filesystem::path file);
void flush();
std::vector<std::string> names();
std::vector<Rule> rules(const std::string &name);
void set(const std::string &name, std::vector<Rule> rules);
void remove(const std::string &name);

} // namespace smart_collections

#endif
