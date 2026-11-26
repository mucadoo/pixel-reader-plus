#include "./cbz_pages.h"

#include <zip.h>

#include <algorithm>
#include <cctype>

namespace
{

std::string lower(const std::string &text)
{
    std::string result = text;
    for (char &c : result)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

bool is_picture_name(const std::string &name)
{
    const std::string low = lower(name);
    const size_t dot = low.rfind('.');
    if (dot == std::string::npos || low.empty() || low.back() == '/')
    {
        return false;
    }
    // Folders such as __MACOSX hold copies that are not pages.
    if (low.find("__macosx") != std::string::npos)
    {
        return false;
    }
    const std::string ext = low.substr(dot);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp";
}

// Names compared with the digit runs as numbers.
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
            // skip leading zeros, then the longer run is the bigger number
            size_t is = i;
            size_t js = j;
            while (is + 1 < ie && a[is] == '0') ++is;
            while (js + 1 < je && b[js] == '0') ++js;
            const size_t la = ie - is;
            const size_t lb = je - js;
            if (la != lb)
            {
                return la < lb;
            }
            const int cmp = a.compare(is, la, b, js, lb);
            if (cmp != 0)
            {
                return cmp < 0;
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
    return a.size() - i < b.size() - j;
}

} // namespace

std::vector<std::string> cbz_page_names(const std::filesystem::path &path)
{
    std::vector<std::string> names;
    int err = 0;
    zip_t *zip = zip_open(path.string().c_str(), ZIP_RDONLY, &err);
    if (!zip)
    {
        return names;
    }
    const zip_int64_t count = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < count; ++i)
    {
        const char *name = zip_get_name(zip, static_cast<zip_uint64_t>(i), 0);
        if (name && is_picture_name(name))
        {
            names.emplace_back(name);
        }
    }
    zip_close(zip);
    std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) {
        return natural_less(lower(a), lower(b));
    });
    return names;
}

std::string cbz_image_format(const std::string &name)
{
    const std::string low = lower(name);
    if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".png") == 0)
    {
        return "PNG";
    }
    if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".gif") == 0)
    {
        return "GIF";
    }
    if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".bmp") == 0)
    {
        return "BMP";
    }
    return "JPG";
}
