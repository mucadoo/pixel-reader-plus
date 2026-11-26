#ifndef STARDICT_H_
#define STARDICT_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

// Reading dictionaries in the StarDict format: a .ifo file that describes it, a .idx file of words with
// where each article is, and a .dict (or dictzip .dict.dz) file with the articles.
namespace stardict
{

struct Info
{
    std::filesystem::path idx;
    std::filesystem::path dict; // .dict or .dict.dz
    bool compressed = false;
    std::string types;          // sametypesequence, empty when each article names its parts
    int offset_bits = 32;
    std::string name;           // bookname
};

// Reads a .ifo file. False when it is not a StarDict description or the other files are missing.
bool read_info(const std::filesystem::path &ifo, Info &out);

struct Entry
{
    std::string key; // folded by the caller's function
    uint64_t offset = 0;
    uint32_t size = 0;
};

// The words of the index, sorted by their folded key. progress gets 0 to 100.
bool read_index(const Info &info, const std::function<std::string(const std::string &)> &fold,
                std::vector<Entry> &out, const std::function<void(int)> &progress = {});

// Gives the text of articles, reading from the file as needed.
class Articles
{
    Info info;
    std::ifstream file;
    // dictzip
    uint32_t chunk_length = 0;
    std::vector<uint64_t> chunk_start; // in the file
    std::vector<uint32_t> chunk_size;  // compressed
    bool ready = false;

    bool open_compressed();
    bool read_raw(uint64_t offset, uint32_t size, std::string &out);

public:
    explicit Articles(Info info);
    // The article at the place the index gives, as plain text. Empty when it cannot be read.
    std::string text(uint64_t offset, uint32_t size);
};

// An article's bytes as readable text, following the types: plain text stays, markup is stripped.
std::string article_to_text(const std::string &raw, const std::string &types);

} // namespace stardict

#endif