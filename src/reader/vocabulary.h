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

// Reviewing the words as flash cards. A word moves up a box each time it is known and back to the
// first box when it is not; the higher the box, the longer until it comes round again.
struct Review
{
    int box = 0;
    long long due = 0; // seconds since 1970, 0 for a word never reviewed
};

constexpr int LAST_BOX = 5;

// How long a word waits in a box before it is due again.
long long interval_seconds(int box);

Review review_of(const std::string &word);
void grade(const std::string &word, bool known, long long now);

// The words due now, newest first.
std::vector<std::string> due_words(long long now);

} // namespace vocabulary

#endif
