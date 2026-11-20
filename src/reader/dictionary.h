#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <optional>
#include <string>
#include <vector>

class Dictionary
{
    bool loaded = false;
    bool ok = false;
    std::string data;
    std::vector<uint32_t> lines;

    void ensure_loaded();
    std::optional<std::string> find_exact(const std::string &key) const;

public:
    std::optional<std::string> lookup(const std::string &key);
    bool available();
};

#endif
