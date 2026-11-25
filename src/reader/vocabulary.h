#ifndef VOCABULARY_H_
#define VOCABULARY_H_

#include <filesystem>
#include <string>
#include <vector>

// Every word looked up in the dictionary, with its meaning and the book it was
// found in. Looking a word up again moves it to the top.
namespace vocabulary
{

struct Entry
{
    std::string word;
    std::string gloss;
    std::string book;
    long long when = 0;
};

void load(std::filesystem::path file);
void flush();

void add(const std::string &word, const std::string &gloss, const std::string &book, long long now);
void remove(size_t index);

// Newest first.
const std::vector<Entry> &entries();

} // namespace vocabulary

#endif
