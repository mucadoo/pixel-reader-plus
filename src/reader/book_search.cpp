#include "./book_search.h"

#include "./accent_fold.h"
#include "./text_style.h"
#include "doc_api/doc_reader.h"
#include "doc_api/doc_token.h"
#include "doc_api/token_iter.h"

#include <algorithm>
#include <cctype>

namespace
{

// Letters and digits, including every byte of a letter outside ASCII.
bool is_word_byte(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80;
}

const std::string *token_text(const DocToken &token)
{
    switch (token.type)
    {
        case TokenType::Text: return &static_cast<const TextDocToken &>(token).text;
        case TokenType::Header: return &static_cast<const HeaderDocToken &>(token).text;
        case TokenType::ListItem: return &static_cast<const ListItemDocToken &>(token).text;
        case TokenType::Image: break;
    }
    return nullptr;
}

bool is_continuation_byte(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

std::string snippet_around(const std::string &text, size_t at, size_t length)
{
    const size_t reach = 34;
    size_t begin = at > reach ? at - reach : 0;
    size_t end = std::min(text.size(), at + length + reach);
    while (begin < at && is_continuation_byte(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }
    while (end < text.size() && is_continuation_byte(static_cast<unsigned char>(text[end])))
    {
        ++end;
    }
    std::string out = text.substr(begin, end - begin);
    for (char &c : out)
    {
        if (c == '\n' || c == '\r' || c == '\t')
        {
            c = ' ';
        }
    }
    return std::string(begin > 0 ? "..." : "") + out + (end < text.size() ? "..." : "");
}

} // namespace

std::vector<SearchHit> search_book(
    const DocReader &reader,
    const std::string &word,
    size_t max_hits,
    const std::function<void(int percent)> &progress,
    const SearchOptions &options
)
{
    std::vector<SearchHit> hits;
    // What both the text and the words typed are turned into before they are compared.
    const auto prepare = [&options](const std::string &raw, std::vector<uint32_t> *map) {
        std::string text = options.match_accents ? raw : fold_accents(raw, map);
        if (!options.match_case)
        {
            if (options.match_accents)
            {
                text = lookup_lowercase(text);
            }
            else
            {
                for (char &c : text)
                {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
        }
        return text;
    };
    const std::string needle = prepare(word, nullptr);
    if (needle.empty())
    {
        return hits;
    }

    auto iter = reader.get_iter(options.from_address);
    if (!iter)
    {
        return hits;
    }

    int last_percent = -1;
    while (const DocToken *token = iter->read(1))
    {
        if (progress)
        {
            const int percent = static_cast<int>(std::min<uint32_t>(reader.get_global_progress_percent(token->address), 100));
            if (percent != last_percent)
            {
                last_percent = percent;
                progress(percent);
            }
        }
        if (options.until_address && token->address >= *options.until_address)
        {
            break;
        }
        const std::string *text = token_text(*token);
        if (!text || text->size() < needle.size() / 2)
        {
            continue;
        }
        std::vector<uint32_t> map;
        const std::string lower = prepare(*text, &map);
        if (lower.size() < needle.size())
        {
            continue;
        }
        size_t from = 0;
        while ((from = lower.find(needle, from)) != std::string::npos)
        {
            const bool before = from > 0 && is_word_byte(static_cast<unsigned char>(lower[from - 1]));
            const size_t after_at = from + needle.size();
            const bool after = after_at < lower.size() && is_word_byte(static_cast<unsigned char>(lower[after_at]));
            if (!options.whole_word || (!before && !after))
            {
                const size_t shown_from = options.match_accents || from >= map.size() ? from : map[from];
                hits.push_back({token->address, snippet_around(*text, shown_from, needle.size())});
                if (hits.size() >= max_hits)
                {
                    return hits;
                }
                // One result for each paragraph keeps the list readable.
                break;
            }
            from = after_at;
        }
    }
    return hits;
}
