#include "./backup.h"

#include <zip.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

namespace backup
{

namespace
{

namespace fs = std::filesystem;

// Files in the store folder that are only caches.
bool is_cache_name(const std::string &name)
{
    return name.size() >= 6 && name.compare(name.size() - 6, 6, ".cache") == 0;
}

bool read_all(const fs::path &file, std::string &out)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// A name that is safe to write inside the store: "file" or "books/file", nothing else.
bool safe_entry_name(const std::string &name)
{
    if (name.empty() || name.find("..") != std::string::npos || name.find('\\') != std::string::npos || name[0] == '/')
    {
        return false;
    }
    const size_t slash = name.find('/');
    if (slash == std::string::npos)
    {
        return true;
    }
    return name.compare(0, slash, "books") == 0 && name.find('/', slash + 1) == std::string::npos && slash + 1 < name.size();
}

} // namespace

Result write(const fs::path &store_dir, const fs::path &zip_file)
{
    Result result;
    std::vector<std::pair<std::string, std::string>> files; // entry name and contents
    std::error_code ec;
    for (const auto &item : fs::directory_iterator(store_dir, ec))
    {
        std::error_code ec_file;
        if (item.is_regular_file(ec_file) && !is_cache_name(item.path().filename().string()))
        {
            std::string data;
            if (read_all(item.path(), data))
            {
                files.emplace_back(item.path().filename().string(), std::move(data));
            }
        }
    }
    for (const auto &item : fs::directory_iterator(store_dir / "books", ec))
    {
        std::error_code ec_file;
        if (item.is_regular_file(ec_file) && !is_cache_name(item.path().filename().string()))
        {
            std::string data;
            if (read_all(item.path(), data))
            {
                files.emplace_back("books/" + item.path().filename().string(), std::move(data));
            }
        }
    }
    if (files.empty())
    {
        result.message = "There is nothing to back up yet.";
        return result;
    }

    int err = 0;
    zip_t *zip = zip_open(zip_file.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!zip)
    {
        result.message = "The backup file could not be created.";
        return result;
    }
    bool added_all = true;
    for (const auto &file : files)
    {
        zip_source_t *source = zip_source_buffer(zip, file.second.data(), file.second.size(), 0);
        if (!source || zip_file_add(zip, file.first.c_str(), source, ZIP_FL_OVERWRITE) < 0)
        {
            if (source)
            {
                zip_source_free(source);
            }
            added_all = false;
            break;
        }
    }
    if (!added_all || zip_close(zip) != 0)
    {
        if (!added_all)
        {
            zip_discard(zip);
        }
        result.message = "The backup could not be written.";
        return result;
    }
    result.ok = true;
    result.files = static_cast<int>(files.size());
    return result;
}

Result restore(const fs::path &store_dir, const fs::path &zip_file)
{
    Result result;
    int err = 0;
    zip_t *zip = zip_open(zip_file.string().c_str(), ZIP_RDONLY, &err);
    if (!zip)
    {
        result.message = "The backup file could not be opened.";
        return result;
    }
    std::error_code ec;
    fs::create_directories(store_dir / "books", ec);
    const zip_int64_t count = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < count; ++i)
    {
        const char *raw_name = zip_get_name(zip, static_cast<zip_uint64_t>(i), 0);
        struct zip_stat st;
        if (!raw_name || zip_stat_index(zip, static_cast<zip_uint64_t>(i), 0, &st) != 0)
        {
            continue;
        }
        const std::string name = raw_name;
        if (!safe_entry_name(name) || name.back() == '/')
        {
            continue;
        }
        zip_file_t *entry = zip_fopen_index(zip, static_cast<zip_uint64_t>(i), 0);
        if (!entry)
        {
            continue;
        }
        std::string data(static_cast<size_t>(st.size), '\0');
        const zip_int64_t got = data.empty() ? 0 : zip_fread(entry, &data[0], data.size());
        zip_fclose(entry);
        if (got != static_cast<zip_int64_t>(data.size()))
        {
            continue;
        }
        std::ofstream out(store_dir / name, std::ios::binary | std::ios::trunc);
        if (out)
        {
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            ++result.files;
        }
    }
    zip_close(zip);
    result.ok = result.files > 0;
    if (!result.ok)
    {
        result.message = "The file has nothing to restore.";
    }
    return result;
}

} // namespace backup
