#include "./vocabulary.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <system_error>

namespace vocabulary
{

namespace
{

std::filesystem::path words_file;
std::vector<Entry> words; // oldest first on disk, newest first in memory
bool dirty = false;
std::map<std::string, Review> reviews;
bool reviews_dirty = false;

std::filesystem::path reviews_file()
{
    return words_file.parent_path() / "vocabulary_review.tsv";
}

void load_reviews()
{
    reviews.clear();
    reviews_dirty = false;
    std::ifstream in(reviews_file());
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // word, box, due
        const size_t last = line.rfind('\t');
        const size_t middle = last == std::string::npos || last == 0 ? std::string::npos : line.rfind('\t', last - 1);
        if (middle == std::string::npos)
        {
            continue;
        }
        try
        {
            Review review;
            review.box = std::clamp(std::stoi(line.substr(middle + 1, last - middle - 1)), 0, LAST_BOX);
            review.due = std::stoll(line.substr(last + 1));
            reviews[line.substr(0, middle)] = review;
        }
        catch (...)
        {
        }
    }
}

void flush_reviews()
{
    if (!reviews_dirty || words_file.empty())
    {
        return;
    }
    std::ofstream out(reviews_file(), std::ios::trunc);
    for (const Entry &entry : words)
    {
        const auto found = reviews.find(entry.word);
        if (found != reviews.end())
        {
            out << entry.word << '\t' << found->second.box << '\t' << found->second.due << '\n';
        }
    }
    reviews_dirty = false;
}

// One line per entry: no tabs or line breaks inside a field.
std::string single_line(std::string text)
{
    for (char &c : text)
    {
        if (c == '\t' || c == '\n' || c == '\r')
        {
            c = ' ';
        }
    }
    return text;
}

} // namespace

void load(std::filesystem::path file)
{
    words_file = std::move(file);
    words.clear();
    std::ifstream in(words_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // time, word, book, gloss
        std::vector<std::string> parts;
        size_t from = 0;
        for (int field = 0; field < 3; ++field)
        {
            const size_t tab = line.find('\t', from);
            if (tab == std::string::npos)
            {
                break;
            }
            parts.push_back(line.substr(from, tab - from));
            from = tab + 1;
        }
        if (parts.size() != 3)
        {
            continue;
        }
        try
        {
            Entry entry;
            entry.when = std::stoll(parts[0]);
            entry.word = parts[1];
            entry.book = parts[2];
            entry.gloss = line.substr(from);
            words.push_back(entry);
        }
        catch (...)
        {
        }
    }
    std::reverse(words.begin(), words.end());
    dirty = false;
    load_reviews();
}

void flush()
{
    if (!dirty || words_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(words_file.parent_path(), ec);
    std::ofstream out(words_file, std::ios::trunc);
    for (auto it = words.rbegin(); it != words.rend(); ++it)
    {
        out << it->when << '\t' << it->word << '\t' << it->book << '\t' << it->gloss << '\n';
    }
    dirty = false;
}

void add(const std::string &word, const std::string &gloss, const std::string &book, long long now)
{
    if (word.empty())
    {
        return;
    }
    Entry entry;
    entry.word = single_line(word);
    entry.gloss = single_line(gloss);
    entry.book = single_line(book);
    entry.when = now;
    words.erase(
        std::remove_if(words.begin(), words.end(), [&entry](const Entry &e) { return e.word == entry.word; }),
        words.end()
    );
    words.insert(words.begin(), entry);
    dirty = true;
    flush();
}

void remove(size_t index)
{
    if (index < words.size())
    {
        words.erase(words.begin() + static_cast<std::ptrdiff_t>(index));
        dirty = true;
        flush();
    }
}

const std::vector<Entry> &entries()
{
    return words;
}

long long interval_seconds(int box)
{
    static const long long days[LAST_BOX + 1] = {0, 1, 3, 7, 14, 30};
    return days[std::clamp(box, 0, LAST_BOX)] * 24 * 60 * 60;
}

Review review_of(const std::string &word)
{
    const auto found = reviews.find(word);
    return found == reviews.end() ? Review() : found->second;
}

void grade(const std::string &word, bool known, long long now)
{
    Review review = review_of(word);
    review.box = known ? std::min(review.box + 1, LAST_BOX) : 0;
    review.due = now + interval_seconds(review.box);
    reviews[word] = review;
    reviews_dirty = true;
    flush_reviews();
}

std::vector<std::string> due_words(long long now)
{
    std::vector<std::string> due;
    for (const Entry &entry : words)
    {
        if (review_of(entry.word).due <= now)
        {
            due.push_back(entry.word);
        }
    }
    return due;
}

} // namespace vocabulary
