#include "reader/next_book.h"

#include "filetypes/open_doc.h"
#include "reader/book_info.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace next_book
{

namespace
{

std::filesystem::path info_dir;
std::unique_ptr<BookInfoCache> cache_instance;
std::function<void(const std::filesystem::path &)> opener_fn;
std::optional<std::filesystem::path> requested;

std::string lower(std::string text)
{
    for (char &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// "Book 2" before "Book 10": digit runs compare as numbers.
bool natural_less(const std::string &a, const std::string &b)
{
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size())
    {
        if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j])))
        {
            size_t ie = i;
            size_t je = j;
            while (ie < a.size() && std::isdigit(static_cast<unsigned char>(a[ie]))) ++ie;
            while (je < b.size() && std::isdigit(static_cast<unsigned char>(b[je]))) ++je;
            std::string na = a.substr(i, ie - i);
            std::string nb = b.substr(j, je - j);
            na.erase(0, std::min(na.find_first_not_of('0'), na.size()));
            nb.erase(0, std::min(nb.find_first_not_of('0'), nb.size()));
            if (na.size() != nb.size())
            {
                return na.size() < nb.size();
            }
            if (na != nb)
            {
                return na < nb;
            }
            i = ie;
            j = je;
        }
        else
        {
            if (a[i] != b[j])
            {
                return a[i] < b[j];
            }
            ++i;
            ++j;
        }
    }
    return (a.size() - i) < (b.size() - j);
}

} // namespace

std::optional<Choice> find_next(const std::filesystem::path &current, BookInfoCache &cache, bool series_only)
{
    std::vector<std::filesystem::path> siblings;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(current.parent_path(), ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code type_ec;
        if (!it->is_regular_file(type_ec) || !file_type_is_supported(it->path()))
        {
            continue;
        }
        siblings.push_back(it->path());
        if (siblings.size() > 400)
        {
            break;
        }
    }

    cache.load(current);
    const BookInfo *mine = cache.get(current);
    if (mine && !mine->series.empty() && mine->series_number() > 0)
    {
        const std::string series = lower(mine->series);
        const double number = mine->series_number();
        std::optional<std::filesystem::path> best;
        double best_number = 0;
        for (const auto &other : siblings)
        {
            if (other == current)
            {
                continue;
            }
            cache.load(other);
            const BookInfo *info = cache.get(other);
            if (!info || lower(info->series) != series || info->series_number() <= number)
            {
                continue;
            }
            if (!best || info->series_number() < best_number)
            {
                best = other;
                best_number = info->series_number();
            }
        }
        if (best)
        {
            return Choice{*best, true};
        }
    }
    if (series_only)
    {
        return std::nullopt;
    }

    std::sort(siblings.begin(), siblings.end(), [](const auto &a, const auto &b) {
        return natural_less(lower(a.filename().string()), lower(b.filename().string()));
    });
    for (size_t i = 0; i + 1 < siblings.size(); ++i)
    {
        if (siblings[i] == current)
        {
            return Choice{siblings[i + 1], false};
        }
    }
    return std::nullopt;
}

void set_info_dir(const std::filesystem::path &dir)
{
    info_dir = dir;
    cache_instance.reset();
}

BookInfoCache &shared_cache()
{
    if (!cache_instance)
    {
        cache_instance = std::make_unique<BookInfoCache>(info_dir);
    }
    return *cache_instance;
}

void set_opener(std::function<void(const std::filesystem::path &)> opener)
{
    opener_fn = std::move(opener);
}

void request(const std::filesystem::path &book)
{
    requested = book;
}

void open_requested()
{
    if (requested && opener_fn)
    {
        const std::filesystem::path book = *requested;
        requested.reset();
        opener_fn(book);
    }
    requested.reset();
}

} // namespace next_book
