#ifndef DOC_READER_H_
#define DOC_READER_H_

#include "./token_iter.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct TocItem
{
    std::string display_name;
    uint32_t indent_level;
};

struct TocPosition
{
    uint32_t toc_index;
    uint32_t progress_percent;
};

// Where a link in the text leads.
struct LinkTarget
{
    DocAddr address = 0;
    // The text found at the target, such as the body of a footnote. Empty when
    // the link points at a place with no text of its own.
    std::string text;
    // True when the link names a spot inside a file, not just a file.
    bool has_fragment = false;
};

// A font file kept inside the book, for one face of the font the book reads in.
struct EmbeddedFont
{
    std::string path; // inside the book, for load_resource
    bool bold = false;
    bool italic = false;
};

// A printed page of the book and where it starts.
struct PageMark
{
    std::string label; // as printed: "12", "xiv"
    DocAddr address = 0;
};

// Allow readers to cache arbitrary data
class DocReaderCache
{
public:
    virtual std::optional<std::string> read(const std::string &book_id, const std::string &key) const = 0;
    virtual void write(const std::string &book_id, const std::string &key, const std::string &value) = 0;
};

// Interface for interacting with a particular document format.
class DocReader
{
public:
    virtual ~DocReader() = default;

    bool open();
    virtual bool open(DocReaderCache &cache) = 0;
    virtual bool is_open() const = 0;

    virtual std::string get_id() const = 0;

    virtual const std::vector<TocItem> &get_table_of_contents() const = 0;
    virtual TocPosition get_toc_position(const DocAddr &address) const = 0;
    virtual DocAddr get_toc_item_address(uint32_t toc_item_index) const = 0;

    virtual uint32_t get_global_progress_percent(const DocAddr &address) const = 0;
    // Position and total length in address units. chapter selects the current chapter.
    virtual std::pair<uint32_t, uint32_t> get_progress_extent(const DocAddr &address, bool chapter) const = 0;
    // The address that lies the given percent (0 to 100) of the way through the book.
    virtual DocAddr get_address_at_percent(uint32_t percent) const = 0;
    // Resolves a link taken from a token's style span. Empty for links that
    // lead outside the book or to nothing that can be found.
    virtual std::optional<LinkTarget> resolve_link(const std::string &href) const = 0;

    virtual std::shared_ptr<TokenIter> get_iter(DocAddr address = 0) const = 0;

    virtual std::vector<char> load_resource(const std::filesystem::path &path) const = 0;

    // The faces of the font the book is set in, when it carries its own. Empty for most books.
    virtual std::vector<EmbeddedFont> embedded_fonts() const { return {}; }

    // The printed pages of the book in order, when it lists them. Empty for most books.
    virtual const std::vector<PageMark> &page_list() const
    {
        static const std::vector<PageMark> none;
        return none;
    }

    // True for a comic book, whose pages are pictures that should be shown whole.
    virtual bool pages_are_pictures() const { return false; }
};

#endif
