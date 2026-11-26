#include "./cbz_reader.h"

#include "./cbz_pages.h"
#include "doc_api/token_addressing.h"
#include "filetypes/txt/txt_token_iter.h"
#include "util/zip_utils.h"

#include "extern/hash-library/md5.h"

#include <zip.h>

#include <algorithm>

struct CbzReaderState
{
    std::filesystem::path path;
    std::vector<std::string> pages;
    std::vector<TocItem> toc; // none: a comic has pages, not chapters
    std::vector<std::unique_ptr<DocToken>> tokens;
    std::string id;
    zip_t *zip = nullptr;
    bool is_open = false;

    explicit CbzReaderState(const std::filesystem::path &path)
        : path(path)
    {
    }
};

CbzReader::CbzReader(const std::filesystem::path &path)
    : state(std::make_unique<CbzReaderState>(path))
{
}

CbzReader::~CbzReader()
{
    if (state->zip)
    {
        zip_close(state->zip);
    }
}

bool CbzReader::open(DocReaderCache &)
{
    if (state->is_open)
    {
        return true;
    }
    state->pages = cbz_page_names(state->path);
    if (state->pages.empty())
    {
        return false;
    }
    int err = 0;
    state->zip = zip_open(state->path.string().c_str(), ZIP_RDONLY, &err);
    if (!state->zip)
    {
        return false;
    }
    // The id follows the names of the pages, so a large file is not read through to name it.
    MD5 md5;
    for (const std::string &name : state->pages)
    {
        md5.add(name.c_str(), name.size());
        md5.add("\n", 1);
    }
    state->id = md5.getHash();

    DocAddr address = 0;
    for (const std::string &name : state->pages)
    {
        auto token = std::make_unique<ImageDocToken>(address, name);
        token->page_break_before = true;
        token->page_break_after = true;
        token->centered = true;
        token->center_set = true;
        address += get_address_width(*token);
        state->tokens.push_back(std::move(token));
    }
    state->is_open = true;
    return true;
}

bool CbzReader::is_open() const
{
    return state->is_open;
}

std::string CbzReader::get_id() const
{
    return state->id;
}

const std::vector<TocItem> &CbzReader::get_table_of_contents() const
{
    return state->toc;
}

TocPosition CbzReader::get_toc_position(const DocAddr &address) const
{
    return {0, get_global_progress_percent(address)};
}

DocAddr CbzReader::get_toc_item_address(uint32_t) const
{
    return 0;
}

uint32_t CbzReader::get_global_progress_percent(const DocAddr &address) const
{
    const uint64_t size = state->tokens.size();
    if (!size)
    {
        return 100;
    }
    // Page n of N shows N/N only on the last page: the percent counts the pages turned past.
    return static_cast<uint32_t>(std::min<uint64_t>(address + 1, size) * 100 / size);
}

DocAddr CbzReader::get_address_at_percent(uint32_t percent) const
{
    const uint64_t size = state->tokens.size();
    if (!size)
    {
        return 0;
    }
    const uint64_t target = size * std::min<uint32_t>(percent, 100) / 100;
    return std::min<uint64_t>(target, size - 1);
}

std::pair<uint32_t, uint32_t> CbzReader::get_progress_extent(const DocAddr &address, bool) const
{
    const uint32_t size = static_cast<uint32_t>(state->tokens.size());
    return {std::min(static_cast<uint32_t>(address), size), size};
}

std::shared_ptr<TokenIter> CbzReader::get_iter(DocAddr address) const
{
    return std::make_shared<TxtTokenIter>(state->tokens, address);
}

std::vector<char> CbzReader::load_resource(const std::filesystem::path &path) const
{
    if (!state->zip)
    {
        return {};
    }
    std::vector<char> data = read_zip_file_str(state->zip, path.string());
    if (!data.empty())
    {
        data.pop_back(); // read_zip_file_str adds a terminating zero
    }
    return data;
}
