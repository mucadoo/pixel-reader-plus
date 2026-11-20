#include "./zip_utils.h"

#include <zip.h>
#include <iostream>
#include <vector>

std::string normalize_zip_path(std::string path)
{
    for (char &c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    std::vector<std::string> parts;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty() || cur == ".")
        {
            cur.clear();
            return;
        }
        if (cur == "..")
        {
            if (!parts.empty())
            {
                parts.pop_back();
            }
        }
        else
        {
            parts.push_back(cur);
        }
        cur.clear();
    };
    for (char c : path)
    {
        if (c == '/')
        {
            flush();
        }
        else
        {
            cur.push_back(c);
        }
    }
    flush();

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
        {
            out.push_back('/');
        }
        out += parts[i];
    }
    return out;
}

// Read zip file contents as a null-terminated string
std::vector<char> read_zip_file_str(zip_t *zip, const std::string &filepath)
{
    if (zip == nullptr)
    {
        throw std::runtime_error("Zip is not open");
    }

    zip_stat_t stats;
    if (zip_stat(zip, filepath.c_str(), 0, &stats) != 0 || !(stats.valid & ZIP_STAT_SIZE))
    {
        std::cerr << "Unable to get size of " << filepath << " in epub" << std::endl;
        return {};
    }

    zip_uint64_t size = stats.size;
    std::vector<char> buffer(size + 1);

    zip_file_t *fp = zip_fopen(zip, filepath.c_str(), 0);
    if (fp == nullptr)
    {
        std::cerr << "Unable to open " << filepath << " in epub" << std::endl;
        return {};
    }

    auto read_size = zip_fread(fp, buffer.data(), size);
    if ((zip_uint64_t)read_size != size)
    {
        std::cerr << "Read unexpected number of bytes for " << filepath << " in epub"
            << " expected " << size
            << " got " << read_size
            << std::endl;
    }

    zip_fclose(fp);

    return buffer;
}
