#include "./book_info.h"

#include "filetypes/epub/epub_metadata.h"
#include "util/zip_utils.h"

#include "extern/hash-library/md5.h"

#include <zip.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

namespace
{

constexpr const char *CACHE_MAGIC = "PXI1";

bool is_epub(const std::filesystem::path &book)
{
    std::string ext = book.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".epub";
}

// One line: no line breaks, no runs of spaces, no spaces at the ends.
std::string tidy(const std::string &text)
{
    std::string out;
    bool space = true; // drops leading spaces
    for (unsigned char c : text)
    {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
        {
            if (!space)
            {
                out.push_back(' ');
                space = true;
            }
            continue;
        }
        out.push_back(static_cast<char>(c));
        space = false;
    }
    while (!out.empty() && out.back() == ' ')
    {
        out.pop_back();
    }
    return out;
}

} // namespace

std::optional<BookInfo> read_epub_info(const std::filesystem::path &book)
{
    if (!is_epub(book))
    {
        return std::nullopt;
    }

    int err = 0;
    zip_t *zip = zip_open(book.c_str(), ZIP_RDONLY, &err);
    if (!zip)
    {
        return std::nullopt;
    }

    std::optional<BookInfo> result;
    try
    {
        auto container = read_zip_file_str(zip, EPUB_CONTAINER_PATH);
        std::string rootfile = container.empty() ? std::string() : epub_parse_rootfile_path(container.data());
        auto package_xml = rootfile.empty() ? std::vector<char>() : read_zip_file_str(zip, rootfile);

        PackageContents package;
        if (!package_xml.empty() && epub_parse_package_contents(rootfile, package_xml.data(), package))
        {
            BookInfo info;
            info.title = tidy(package.title);
            info.author = tidy(package.author);
            info.language = tidy(package.language);
            if (!info.empty())
            {
                result = info;
            }
        }
    }
    catch (...)
    {
        result = std::nullopt;
    }

    zip_close(zip);
    return result;
}

std::string book_display_title(const std::optional<BookInfo> &info, const std::filesystem::path &book)
{
    if (info && !info->title.empty())
    {
        return info->title;
    }
    std::string stem = book.stem().string();
    return stem.empty() ? book.filename().string() : stem;
}

BookInfoCache::BookInfoCache(std::filesystem::path dir)
    : dir(std::move(dir))
{
    std::error_code ec;
    std::filesystem::create_directories(this->dir, ec);
}

std::string BookInfoCache::cache_key(const std::filesystem::path &book) const
{
    std::error_code ec;
    auto size = std::filesystem::file_size(book, ec);
    if (ec)
    {
        return "";
    }
    auto modified = std::filesystem::last_write_time(book, ec);
    if (ec)
    {
        return "";
    }
    std::string text = book.string()
        + "|" + std::to_string(static_cast<unsigned long long>(size))
        + "|" + std::to_string(static_cast<long long>(modified.time_since_epoch().count()));
    return MD5()(text);
}

bool BookInfoCache::has(const std::filesystem::path &book) const
{
    return known.count(book.string()) > 0;
}

const BookInfo *BookInfoCache::get(const std::filesystem::path &book) const
{
    auto it = known.find(book.string());
    if (it == known.end() || !it->second)
    {
        return nullptr;
    }
    return &*it->second;
}

void BookInfoCache::load(const std::filesystem::path &book)
{
    const std::string path = book.string();
    if (known.count(path))
    {
        return;
    }

    const std::string key = cache_key(book);
    if (key.empty() || !is_epub(book))
    {
        known[path] = std::nullopt;
        return;
    }

    const std::filesystem::path file = dir / (key + ".info");
    {
        std::ifstream in(file);
        std::string magic;
        if (in && std::getline(in, magic) && magic == CACHE_MAGIC)
        {
            BookInfo info;
            std::getline(in, info.title);
            std::getline(in, info.author);
            std::getline(in, info.language);
            known[path] = info.empty() ? std::nullopt : std::optional<BookInfo>(info);
            return;
        }
    }

    std::optional<BookInfo> info = read_epub_info(book);
    known[path] = info;

    std::filesystem::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
        {
            return;
        }
        BookInfo shown = info.value_or(BookInfo {});
        out << CACHE_MAGIC << '\n' << shown.title << '\n' << shown.author << '\n' << shown.language << '\n';
        if (!out)
        {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, file, ec);
}
