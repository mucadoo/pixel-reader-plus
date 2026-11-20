#include "./dictionary.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{

const char *CANDIDATES[] = {
    "resources/dictionary/english.txt",
    "/mnt/SDCARD/App/PixelReader/resources/dictionary/english.txt",
};

int compare_key(const std::string &word, const char *start, size_t length)
{
    size_t n = std::min(word.size(), length);
    int cmp = std::memcmp(word.data(), start, n);
    if (cmp != 0)
    {
        return cmp;
    }
    if (word.size() == length)
    {
        return 0;
    }
    return word.size() < length ? -1 : 1;
}

} // namespace

void Dictionary::ensure_loaded()
{
    if (loaded)
    {
        return;
    }
    loaded = true;

    std::ifstream in;
    for (const char *path : CANDIDATES)
    {
        in.open(path, std::ios::binary);
        if (in)
        {
            break;
        }
        in.clear();
    }
    if (!in)
    {
        return;
    }

    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    if (size <= 0)
    {
        return;
    }
    in.seekg(0);
    data.resize(static_cast<size_t>(size));
    in.read(data.data(), size);
    if (data.empty())
    {
        return;
    }

    lines.push_back(0);
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (data[i] == '\n' && i + 1 < data.size())
        {
            lines.push_back(static_cast<uint32_t>(i + 1));
        }
    }
    ok = !lines.empty();
}

std::optional<std::string> Dictionary::find_exact(const std::string &key) const
{
    if (!ok || key.empty())
    {
        return std::nullopt;
    }

    size_t lo = 0;
    size_t hi = lines.size();
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        size_t start = lines[mid];
        size_t tab = data.find('\t', start);
        size_t eol = data.find('\n', start);
        if (tab == std::string::npos || (eol != std::string::npos && tab > eol))
        {
            return std::nullopt;
        }
        int cmp = compare_key(key, data.data() + start, tab - start);
        if (cmp == 0)
        {
            size_t gloss_end = eol == std::string::npos ? data.size() : eol;
            if (gloss_end > 0 && data[gloss_end - 1] == '\r')
            {
                --gloss_end;
            }
            return data.substr(tab + 1, gloss_end - (tab + 1));
        }
        if (cmp < 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return std::nullopt;
}

std::optional<std::string> Dictionary::lookup(const std::string &key)
{
    ensure_loaded();
    if (auto found = find_exact(key))
    {
        return found;
    }
    if (key.size() > 2 && key.compare(key.size() - 2, 2, "'s") == 0)
    {
        if (auto found = find_exact(key.substr(0, key.size() - 2)))
        {
            return found;
        }
    }
    if (key.size() > 3 && key.back() == 's')
    {
        if (auto found = find_exact(key.substr(0, key.size() - 1)))
        {
            return found;
        }
    }
    return std::nullopt;
}

bool Dictionary::available()
{
    ensure_loaded();
    return ok;
}
