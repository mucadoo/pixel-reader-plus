#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <vector>
#include <string>

struct FSEntry
{
    std::string name;
    bool is_dir;
    unsigned long long size = 0; // bytes, files only

    static FSEntry directory(const std::string& path)
    {
        return { path, true, 0 };
    }
};

std::vector<FSEntry> directory_listing(const std::string& path);

#endif
