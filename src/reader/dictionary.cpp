#include "./dictionary.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace
{

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

Dictionary::Dictionary(std::string name) : name(std::move(name))
{
}

std::string dictionary_name_for_language(const std::string &language)
{
    std::string code;
    for (char c : language)
    {
        if (c == '-' || c == '_')
        {
            break;
        }
        code.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (code == "pt" || code == "por")
    {
        return "portuguese";
    }
    if (code == "fr" || code == "fra" || code == "fre")
    {
        return "french";
    }
    return "english";
}

std::vector<std::string> dictionary_names()
{
    std::set<std::string> names;
    const char *folders[] = {"resources/dictionary", "/mnt/SDCARD/App/PixelReader/resources/dictionary"};
    for (const char *folder : folders)
    {
        std::error_code ec;
        for (const auto &item : std::filesystem::directory_iterator(folder, ec))
        {
            std::error_code ec_file;
            if (!item.is_regular_file(ec_file) || item.path().extension() != ".txt")
            {
                continue;
            }
            std::string name = item.path().stem().string();
            std::string lower = name;
            for (char &c : lower)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (lower.compare(0, 7, "license") != 0)
            {
                names.insert(name);
            }
        }
    }
    return std::vector<std::string>(names.begin(), names.end());
}
void Dictionary::ensure_loaded(const std::function<void(int)> &progress)
{
    if (loaded)
    {
        return;
    }
    loaded = true;

    const std::string candidates[] = {
        "resources/dictionary/" + name + ".txt",
        "/mnt/SDCARD/App/PixelReader/resources/dictionary/" + name + ".txt",
    };
    std::ifstream in;
    for (const std::string &path : candidates)
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
    const size_t total = static_cast<size_t>(size);
    data.resize(total);

    // Read in pieces so progress can be shown, and find the line starts as we go.
    const size_t piece = 1 << 20;
    lines.push_back(0);
    size_t done = 0;
    while (done < total)
    {
        const size_t want = std::min(piece, total - done);
        in.read(data.data() + done, static_cast<std::streamsize>(want));
        const size_t got = static_cast<size_t>(in.gcount());
        if (got == 0)
        {
            break;
        }
        const char *begin = data.data() + done;
        const char *end = begin + got;
        for (const char *p = begin; (p = static_cast<const char *>(std::memchr(p, '\n', static_cast<size_t>(end - p)))); ++p)
        {
            lines.push_back(static_cast<uint32_t>(p - data.data() + 1));
        }
        done += got;
        if (progress)
        {
            progress(static_cast<int>(done * 100 / total));
        }
    }
    data.resize(done);
    if (!lines.empty() && lines.back() >= data.size())
    {
        lines.pop_back();
    }
    ok = done > 0 && !lines.empty();

    // Dictionaries that came with the reader are sorted. One of your own may not be.
    if (ok && name != "english" && name != "french" && name != "portuguese")
    {
        const auto key_end = [this](uint32_t start) {
            size_t tab = data.find('\t', start);
            const size_t eol = data.find('\n', start);
            if (tab == std::string::npos || (eol != std::string::npos && tab > eol))
            {
                tab = eol == std::string::npos ? data.size() : eol;
            }
            return tab;
        };
        const auto less = [this, &key_end](uint32_t a, uint32_t b) {
            const size_t ea = key_end(a);
            const size_t eb = key_end(b);
            return compare_key(std::string(data, a, ea - a), data.data() + b, eb - b) < 0;
        };
        if (!std::is_sorted(lines.begin(), lines.end(), less))
        {
            std::stable_sort(lines.begin(), lines.end(), less);
        }
    }
}

void Dictionary::load(const std::function<void(int)> &progress)
{
    ensure_loaded(progress);
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
    // l'homme, qu'il: try the part after the apostrophe.
    const size_t apostrophe = key.rfind('\'');
    if (apostrophe != std::string::npos && apostrophe + 1 < key.size())
    {
        if (auto found = find_exact(key.substr(apostrophe + 1)))
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
