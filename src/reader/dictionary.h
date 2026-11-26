#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace stardict { class Articles; }

// An offline dictionary: one file of "word<TAB>gloss" lines sorted by word.
class Dictionary
{
    std::string name;
    bool loaded = false;
    bool ok = false;
    std::string data;
    std::vector<uint32_t> lines;
    std::shared_ptr<stardict::Articles> articles; // set for a StarDict dictionary

    bool load_stardict(const std::function<void(int)> &progress);
    std::optional<std::string> resolve(std::string gloss) const;

    void ensure_loaded(const std::function<void(int)> &progress = {});
    std::optional<std::string> find_exact(const std::string &key) const;

public:
    // name picks resources/dictionary/<name>.txt
    explicit Dictionary(std::string name = "english");

    std::optional<std::string> lookup(const std::string &key);
    bool available();
    // Words close to a key that has no entry: the key without a common ending, words that start with
    // it, and words one letter away from it. Best guesses first.
    std::vector<std::string> suggest(const std::string &key, size_t max_count = 8);
    bool is_loaded() const { return loaded; }
    // Reads the file now, reporting 0 to 100 as it goes. A later lookup then answers at once.
    void load(const std::function<void(int)> &progress);
};

// The names of the dictionaries on the card: every .txt file in resources/dictionary except the
// license notes, and every StarDict dictionary (a folder with a .ifo file in it, or a .ifo file with its
// .idx and .dict or .dict.dz beside it). Drop a file of "word<TAB>meaning" lines in that folder to add your own; words are
// written in lowercase, and the file need not be sorted.
std::vector<std::string> dictionary_names();

// The dictionary that suits a book language such as "pt-BR" or "fr". Unknown
// languages get the English one.
std::string dictionary_name_for_language(const std::string &language);

#endif