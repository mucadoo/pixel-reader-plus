#include "./filesystem.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

std::vector<FSEntry> directory_listing(const std::string& path)
{
    DIR* dir = opendir(path.c_str());
    if (dir == NULL) {
        return {};
    }

    std::vector<FSEntry> entries;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {

        if (entry && strlen(entry->d_name))
        {
            bool is_dir = entry->d_type == DT_DIR;
            bool is_file = entry->d_type == DT_REG;
            std::string name = std::string(entry->d_name);
            if ((is_dir || is_file) && name != "." && name != "..")
            {
                unsigned long long size = 0;
                if (is_file)
                {
                    struct stat st;
                    if (fstatat(dirfd(dir), entry->d_name, &st, 0) == 0)
                    {
                        size = static_cast<unsigned long long>(st.st_size);
                    }
                }
                entries.push_back({name, is_dir, size});
            }
        }
    }

    closedir(dir);

    std::sort(entries.begin(), entries.end(), [](const FSEntry& a, const FSEntry& b) {
        if (a.is_dir != b.is_dir) {
            return a.is_dir > b.is_dir;
        }
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    return entries;
}
