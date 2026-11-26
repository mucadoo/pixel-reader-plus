#ifndef OPDS_H_
#define OPDS_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Getting books from an OPDS catalog (the format of Calibre-web, Kavita, COPS and
// many free libraries) over plain HTTP. There is no TLS in the reader, so https
// addresses are refused with a message.
namespace opds
{

struct Entry
{
    std::string title;
    std::string author;
    std::string href;       // the next catalog to open, or the book to download
    std::string extension;  // of the book, such as "epub"; empty for a catalog
    bool is_book = false;
};

struct Feed
{
    std::string title;
    std::vector<Entry> entries;
    std::string next;       // the next page of the same list, empty on the last
};

struct Url
{
    bool ok = false;
    std::string error;
    std::string host;
    std::string port = "80";
    std::string path = "/";
    std::string credentials; // user:password, empty when the address has none
};

Url parse_url(const std::string &url);

// An address from a link found on the page at base.
std::string resolve(const std::string &base, const std::string &href);

// Called with the bytes received so far and the total, or 0 when it is not known.
using Progress = std::function<void(size_t received, size_t total)>;

// Reads a page into body. Follows a few redirects. False with an error message when it fails.
bool fetch(const std::string &url, std::string &body, std::string &error, const Progress &progress = nullptr);

// Saves a file. A partial file is removed when the transfer fails.
bool download(const std::string &url, const std::filesystem::path &file, std::string &error, const Progress &progress = nullptr);

// Reads a catalog page. base is the address it came from, so links can be made complete.
bool parse_feed(const std::string &xml, const std::string &base, Feed &feed, std::string &error);

// A file name for a book that is safe on the SD card, without a folder.
std::string file_name_for(const std::string &title, const std::string &author, const std::string &extension);

// Decodes the body of a chunked transfer. Exposed for the tests.
class ChunkDecoder
{
public:
    // Feeds bytes; decoded data goes to out. False when the stream is malformed.
    bool feed(const char *data, size_t length, std::string &out);
    bool finished() const { return state == State::Done; }

private:
    enum class State { Size, Data, DataEnd, Trailer, Done };
    State state = State::Size;
    std::string line;
    size_t remaining = 0;
};

} // namespace opds

#endif
