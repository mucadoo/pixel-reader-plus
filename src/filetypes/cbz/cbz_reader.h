#ifndef CBZ_READER_H_
#define CBZ_READER_H_

#include "doc_api/doc_reader.h"

#include <memory>

struct CbzReaderState;

// A comic book: a zip of pictures, one picture per page.
class CbzReader: public DocReader
{
    std::unique_ptr<CbzReaderState> state;

public:
    CbzReader(const std::filesystem::path &path);
    CbzReader(const CbzReader &) = delete;
    CbzReader &operator=(const CbzReader &) = delete;

    virtual ~CbzReader();

    using DocReader::open;
    bool open(DocReaderCache &) override;
    bool is_open() const override;

    std::string get_id() const override;

    const std::vector<TocItem> &get_table_of_contents() const override;
    TocPosition get_toc_position(const DocAddr &address) const override;
    DocAddr get_toc_item_address(uint32_t toc_item_index) const override;

    uint32_t get_global_progress_percent(const DocAddr &address) const override;
    std::pair<uint32_t, uint32_t> get_progress_extent(const DocAddr &address, bool chapter) const override;
    DocAddr get_address_at_percent(uint32_t percent) const override;
    std::optional<LinkTarget> resolve_link(const std::string &) const override { return std::nullopt; }

    std::shared_ptr<TokenIter> get_iter(DocAddr address = 0) const override;

    std::vector<char> load_resource(const std::filesystem::path &path) const override;

    bool pages_are_pictures() const override { return true; }
};

#endif
