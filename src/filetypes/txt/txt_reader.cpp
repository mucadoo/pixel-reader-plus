#include "./txt_reader.h"
#include "./txt_token_iter.h"
#include "doc_api/token_addressing.h"
#include "filetypes/text/markup.h"
#include "util/str_utils.h"

#include "extern/hash-library/md5.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

namespace
{

constexpr uint32_t SPACES_PER_TAB = 4;

// Adds a block as a token. Headings also become chapters.
void add_block_token(
    const markup::Block &block,
    DocAddr &address,
    std::vector<std::unique_ptr<DocToken>> &tokens_out,
    std::vector<TocItem> &toc_out,
    std::vector<DocAddr> &toc_addresses_out,
    int &min_heading_level
)
{
    std::unique_ptr<DocToken> token;
    switch (block.kind)
    {
        case markup::Block::Kind::Heading:
        {
            auto header = std::make_unique<HeaderDocToken>(address, block.text);
            header->header_level = std::max(1, std::min(6, block.level));
            token = std::move(header);
            std::string name = block.text;
            for (auto &c : name)
            {
                if (c == '\n')
                {
                    c = ' ';
                }
            }
            toc_out.push_back({name, static_cast<uint32_t>(block.level)});
            toc_addresses_out.push_back(address);
            min_heading_level = std::min(min_heading_level, block.level);
            break;
        }
        case markup::Block::Kind::ListItem:
            token = std::make_unique<ListItemDocToken>(address, block.text, std::max(1, block.level));
            break;
        case markup::Block::Kind::Blank:
            token = std::make_unique<TextDocToken>(address, "");
            break;
        case markup::Block::Kind::Pre:
            token = std::make_unique<TextDocToken>(address, block.text);
            token->pre = true;
            break;
        case markup::Block::Kind::Paragraph:
        default:
            token = std::make_unique<TextDocToken>(address, block.text);
            break;
    }
    token->spans = block.spans;
    token->quote = block.quote;
    if (block.centered && block.kind != markup::Block::Kind::Heading)
    {
        token->centered = true;
        token->center_set = true;
    }
    address += get_address_width(*token);
    tokens_out.push_back(std::move(token));
}

bool tokenize_text_file(
    const std::filesystem::path &path,
    std::vector<std::unique_ptr<DocToken>> &tokens_out,
    std::string &md5_out,
    std::vector<TocItem> &toc_out,
    std::vector<DocAddr> &toc_addresses_out
)
{
    std::string raw;
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        raw = contents.str();
    }

    DocAddr cur_address = 0;

    markup::Format format;
    if (markup::format_for_extension(path.extension().string(), format))
    {
        md5_out = MD5()(raw);
        const std::vector<markup::Block> blocks = markup::parse(markup::to_utf8(raw), format);
        int min_level = 7;
        for (const auto &block : blocks)
        {
            add_block_token(block, cur_address, tokens_out, toc_out, toc_addresses_out, min_level);
        }
        for (auto &item : toc_out)
        {
            item.indent_level = item.indent_level > static_cast<uint32_t>(min_level) ? item.indent_level - min_level : 0;
        }
        return true;
    }

    // Plain text: every line is a paragraph. The id stays what it always was.
    {
        MD5 md5;
        std::istringstream lines(raw);
        std::string line;
        while (std::getline(lines, line))
        {
            md5.add(line.c_str(), line.size());
            md5.add("\n", 1);
        }
        md5_out = md5.getHash();
    }

    std::istringstream file(markup::to_utf8(raw));
    std::string line;
    while (std::getline(file, line))
    {
        line = strip_whitespace_right(
            convert_tabs_to_space(
                remove_carriage_returns(line),
                SPACES_PER_TAB
            )
        );

        tokens_out.emplace_back(std::make_unique<TextDocToken>(cur_address, line));

        cur_address += get_address_width(*tokens_out.back().get());
    }

    return true;
}
} // namespace

struct TxtReaderState
{
    std::filesystem::path path;
    std::vector<TocItem> toc;
    std::vector<DocAddr> toc_addresses; // where each chapter starts, parallel to toc
    std::vector<std::unique_ptr<DocToken>> tokens;
    std::string md5;
    bool is_open = false;
    uint32_t total_address_width = 0;

    TxtReaderState(const std::filesystem::path &path)
        : path(path)
    {
    }
};

TxtReader::TxtReader(const std::filesystem::path &path)
    : state(std::make_unique<TxtReaderState>(path))
{
}

TxtReader::~TxtReader()
{
}

bool TxtReader::open(DocReaderCache &)
{
    if (state->is_open)
    {
        return true;
    }
    state->is_open = tokenize_text_file(state->path, state->tokens, state->md5, state->toc, state->toc_addresses);
    if (state->is_open && state->tokens.size())
    {
        const auto *last_token = state->tokens.back().get();
        state->total_address_width = last_token->address + get_address_width(*last_token);
    }

    return state->is_open;
}

bool TxtReader::is_open() const
{
    return state->is_open;
}

std::string TxtReader::get_id() const
{
    return state->md5;
}

const std::vector<TocItem> &TxtReader::get_table_of_contents() const
{
    return state->toc;
}

// The chapter the address is in, with where it starts and ends. Only when the book has chapters.
static bool chapter_bounds(const TxtReaderState &state, const DocAddr &address, uint32_t &index, uint32_t &start, uint32_t &end)
{
    if (state.toc_addresses.empty())
    {
        return false;
    }
    index = 0;
    for (uint32_t i = 0; i < state.toc_addresses.size(); ++i)
    {
        if (state.toc_addresses[i] <= address)
        {
            index = i;
        }
        else
        {
            break;
        }
    }
    start = static_cast<uint32_t>(state.toc_addresses[index]);
    end = index + 1 < state.toc_addresses.size()
        ? static_cast<uint32_t>(state.toc_addresses[index + 1])
        : state.total_address_width;
    if (end < start)
    {
        end = start;
    }
    return true;
}

TocPosition TxtReader::get_toc_position(const DocAddr &address) const
{
    uint32_t index = 0, start = 0, end = 0;
    if (!chapter_bounds(*state, address, index, start, end))
    {
        return {0, get_global_progress_percent(address)};
    }
    const uint32_t pos = std::min(std::max(static_cast<uint32_t>(address), start), end);
    return {index, end > start ? (pos - start) * 100 / (end - start) : 100};
}

uint32_t TxtReader::get_global_progress_percent(const DocAddr &address) const
{
    uint32_t pos = address;
    uint32_t size = state->total_address_width;

    if (!size)
    {
        return 100;
    }

    return std::min(pos, size) * 100 / size;
}

DocAddr TxtReader::get_address_at_percent(uint32_t percent) const
{
    const uint64_t size = state->total_address_width;
    const uint64_t target = size * std::min<uint32_t>(percent, 100) / 100;
    return target >= size && size > 0 ? size - 1 : target;
}

std::pair<uint32_t, uint32_t> TxtReader::get_progress_extent(const DocAddr &address, bool chapter) const
{
    uint32_t index = 0, start = 0, end = 0;
    if (chapter && chapter_bounds(*state, address, index, start, end))
    {
        const uint32_t pos = std::min(std::max(static_cast<uint32_t>(address), start), end);
        return {pos - start, end - start};
    }
    uint32_t size = state->total_address_width;
    uint32_t pos = std::min(static_cast<uint32_t>(address), size);
    return {pos, size};
}

DocAddr TxtReader::get_toc_item_address(uint32_t toc_item_index) const
{
    return toc_item_index < state->toc_addresses.size() ? state->toc_addresses[toc_item_index] : 0;
}

std::shared_ptr<TokenIter> TxtReader::get_iter(DocAddr address) const
{
    return std::make_shared<TxtTokenIter>(state->tokens, address);
}

std::vector<char> TxtReader::load_resource(const std::filesystem::path &) const
{
    throw std::runtime_error("Load resource is not supported for txt");
}
