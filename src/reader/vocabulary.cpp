#include "./vocabulary.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace vocabulary
{

namespace
{

std::filesystem::path words_file;
std::vector<Entry> words; // oldest first on disk, newest first in memory
bool dirty = false;

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

} // namespace vocabulary
