#include "./calibre_sidecar.h"

#include "filetypes/open_doc.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

namespace calibre_sidecar
{

namespace
{

std::string lowercase(std::string text)
{
    for (char &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// Whether the book is the only supported book in its folder.
bool alone_in_folder(const std::filesystem::path &book)
{
    std::error_code ec;
    int books = 0;
    for (std::filesystem::directory_iterator it(book.parent_path(), ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code ec_file;
        if (it->is_regular_file(ec_file) && file_type_is_supported(it->path()))
        {
            if (++books > 1)
            {
                return false;
            }
        }
    }
    return books == 1;
}

// A file in the folder whose name, without case, is one of the wanted names.
std::optional<std::filesystem::path> find_named(const std::filesystem::path &folder, const std::string &stem, const std::initializer_list<const char *> &extensions)
{
    std::error_code ec;
    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code ec_file;
        if (!it->is_regular_file(ec_file))
        {
            continue;
        }
        const std::string name = lowercase(it->path().filename().string());
        for (const char *extension : extensions)
        {
            if (name == lowercase(stem) + extension)
            {
                return it->path();
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::filesystem::path> opf_for(const std::filesystem::path &book)
{
    const std::filesystem::path folder = book.parent_path();
    if (auto own = find_named(folder, book.stem().string(), {".opf"}))
    {
        return own;
    }
    if (alone_in_folder(book))
    {
        return find_named(folder, "metadata", {".opf"});
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> cover_for(const std::filesystem::path &book)
{
    const std::filesystem::path folder = book.parent_path();
    if (auto own = find_named(folder, book.stem().string(), {".jpg", ".jpeg", ".png"}))
    {
        return own;
    }
    if (alone_in_folder(book))
    {
        return find_named(folder, "cover", {".jpg", ".jpeg", ".png"});
    }
    return std::nullopt;
}

} // namespace calibre_sidecar