#include "./storage_usage.h"

#include <sys/statvfs.h>

#include <cstdio>
#include <system_error>
#include <vector>

namespace storage_usage
{

namespace
{

bool ends_with(const std::string &text, const std::string &suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

std::uintmax_t folder_size(const std::filesystem::path &folder)
{
    std::uintmax_t total = 0;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        std::error_code ec_file;
        if (it->is_regular_file(ec_file))
        {
            const auto size = it->file_size(ec_file);
            if (!ec_file)
            {
                total += size;
            }
        }
    }
    return total;
}

std::uintmax_t files_size(const std::filesystem::path &folder, const std::string &suffix, bool want_suffix)
{
    std::uintmax_t total = 0;
    std::error_code ec;
    for (const auto &item : std::filesystem::directory_iterator(folder, ec))
    {
        std::error_code ec_file;
        if (!item.is_regular_file(ec_file) || (!suffix.empty() && ends_with(item.path().filename().string(), suffix) != want_suffix))
        {
            continue;
        }
        const auto size = item.file_size(ec_file);
        if (!ec_file)
        {
            total += size;
        }
    }
    return total;
}

std::uintmax_t clear_folder(const std::filesystem::path &folder)
{
    const std::uintmax_t before = folder_size(folder);
    std::error_code ec;
    for (const auto &item : std::filesystem::directory_iterator(folder, ec))
    {
        std::error_code ec_remove;
        std::filesystem::remove_all(item.path(), ec_remove);
    }
    return before - folder_size(folder);
}

std::uintmax_t clear_files(const std::filesystem::path &folder, const std::string &suffix, bool want_suffix)
{
    std::uintmax_t freed = 0;
    std::error_code ec;
    std::vector<std::filesystem::path> doomed;
    for (const auto &item : std::filesystem::directory_iterator(folder, ec))
    {
        std::error_code ec_file;
        if (!item.is_regular_file(ec_file) || (!suffix.empty() && ends_with(item.path().filename().string(), suffix) != want_suffix))
        {
            continue;
        }
        doomed.push_back(item.path());
    }
    for (const auto &path : doomed)
    {
        std::error_code ec_file;
        const auto size = std::filesystem::file_size(path, ec_file);
        if (std::filesystem::remove(path, ec_file) && !ec_file)
        {
            freed += size;
        }
    }
    return freed;
}

std::string format_bytes(std::uintmax_t bytes)
{
    char text[32];
    if (bytes < 1024)
    {
        std::snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
    }
    else if (bytes < 1024ull * 1024)
    {
        std::snprintf(text, sizeof(text), "%llu KB", static_cast<unsigned long long>((bytes + 512) / 1024));
    }
    else
    {
        std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return text;
}

long long free_bytes(const std::filesystem::path &path)
{
    struct statvfs info;
    if (statvfs(path.c_str(), &info) != 0)
    {
        return -1;
    }
    return static_cast<long long>(info.f_bavail) * static_cast<long long>(info.f_frsize);
}

} // namespace storage_usage
