#include "./stardict.h"

#include "extern/stb/stb_image.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace stardict
{

namespace
{

std::string trim(std::string text)
{
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
    {
        text.pop_back();
    }
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t'))
    {
        ++start;
    }
    return text.substr(start);
}

uint32_t be32(const unsigned char *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

std::string decode_entity(const std::string &name)
{
    static const struct { const char *name; const char *text; } known[] = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", " "},
        {"ndash", "\xE2\x80\x93"}, {"mdash", "\xE2\x80\x94"}, {"hellip", "\xE2\x80\xA6"},
        {"lsquo", "\xE2\x80\x98"}, {"rsquo", "\xE2\x80\x99"}, {"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"},
    };
    for (const auto &item : known)
    {
        if (name == item.name)
        {
            return item.text;
        }
    }
    if (name.size() > 1 && name[0] == '#')
    {
        const bool hex = name.size() > 2 && (name[1] == 'x' || name[1] == 'X');
        const long code = std::strtol(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10);
        std::string out;
        if (code > 0 && code < 0x110000)
        {
            if (code < 0x80) out.push_back(static_cast<char>(code));
            else if (code < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }
        return out;
    }
    return std::string();
}

// Markup (html, xdxf, pango) as text: tags go, entities are decoded, block tags make line breaks.
std::string strip_markup(const std::string &markup)
{
    std::string out;
    for (size_t i = 0; i < markup.size(); ++i)
    {
        const char c = markup[i];
        if (c == '<')
        {
            const size_t close = markup.find('>', i);
            if (close == std::string::npos)
            {
                out.push_back(c);
                continue;
            }
            std::string tag = markup.substr(i + 1, close - i - 1);
            std::string name;
            for (char t : tag)
            {
                if (t == ' ' || t == '/' || t == '\t' || t == '\n')
                {
                    if (!name.empty()) break;
                    if (t == '/') continue;
                }
                name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(t))));
            }
            if (name == "br" || name == "p" || name == "div" || name == "li" || name == "tr" || name == "k" || name == "def" || name == "ex")
            {
                if (!out.empty() && out.back() != '\n')
                {
                    out.push_back('\n');
                }
            }
            i = close;
        }
        else if (c == '&')
        {
            const size_t semi = markup.find(';', i);
            if (semi != std::string::npos && semi - i <= 10)
            {
                const std::string text = decode_entity(markup.substr(i + 1, semi - i - 1));
                if (!text.empty())
                {
                    out += text;
                    i = semi;
                    continue;
                }
            }
            out.push_back(c);
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

std::string tidy(const std::string &text)
{
    // No runs of blank lines, no trailing spaces.
    std::string out;
    int newlines = 0;
    for (char c : text)
    {
        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            while (!out.empty() && out.back() == ' ')
            {
                out.pop_back();
            }
            if (!out.empty() && newlines < 1)
            {
                out.push_back('\n');
            }
            ++newlines;
            continue;
        }
        if (c == ' ' && (out.empty() || out.back() == '\n' || out.back() == ' '))
        {
            continue;
        }
        newlines = 0;
        out.push_back(c);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
    {
        out.pop_back();
    }
    return out;
}

std::string part_to_text(char type, const std::string &data)
{
    switch (type)
    {
        case 'm':
        case 'l':
        case 'g':
        case 't':
            return type == 'g' ? strip_markup(data) : data;
        case 'h':
        case 'x':
        case 'y':
        case 'w':
            return strip_markup(data);
        default:
            return std::string(); // pictures, sounds and the rest
    }
}

} // namespace

bool read_info(const std::filesystem::path &ifo, Info &out)
{
    std::ifstream in(ifo, std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::string line;
    std::getline(in, line);
    if (line.compare(0, 8, "StarDict") != 0)
    {
        return false;
    }
    while (std::getline(in, line))
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "sametypesequence")
        {
            out.types = value;
        }
        else if (key == "idxoffsetbits")
        {
            out.offset_bits = value == "64" ? 64 : 32;
        }
        else if (key == "bookname")
        {
            out.name = value;
        }
    }
    const std::filesystem::path base = ifo.parent_path() / ifo.stem();
    std::error_code ec;
    out.idx = base;
    out.idx += ".idx";
    out.dict = base;
    out.dict += ".dict";
    out.compressed = false;
    if (!std::filesystem::exists(out.dict, ec))
    {
        out.dict = base;
        out.dict += ".dict.dz";
        out.compressed = true;
    }
    return std::filesystem::exists(out.idx, ec) && std::filesystem::exists(out.dict, ec);
}

bool read_index(const Info &info, const std::function<std::string(const std::string &)> &fold,
                std::vector<Entry> &out, const std::function<void(int)> &progress)
{
    std::ifstream in(info.idx, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size <= 0)
    {
        return false;
    }
    in.seekg(0);
    std::string data(static_cast<size_t>(size), '\0');
    in.read(data.data(), size);
    const size_t total = static_cast<size_t>(in.gcount());
    const size_t number_size = info.offset_bits == 64 ? 12 : 8;
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(data.data());
    size_t at = 0;
    size_t last_percent = 0;
    while (at < total)
    {
        const char *end = static_cast<const char *>(std::memchr(data.data() + at, '\0', total - at));
        if (!end)
        {
            break;
        }
        const size_t key_length = static_cast<size_t>(end - (data.data() + at));
        const size_t numbers = at + key_length + 1;
        if (numbers + number_size > total)
        {
            break;
        }
        Entry entry;
        entry.key = fold(std::string(data.data() + at, key_length));
        for (char &c : entry.key)
        {
            if (c == '\t' || c == '\n' || c == '\r')
            {
                c = ' ';
            }
        }
        if (info.offset_bits == 64)
        {
            entry.offset = (static_cast<uint64_t>(be32(bytes + numbers)) << 32) | be32(bytes + numbers + 4);
            entry.size = be32(bytes + numbers + 8);
        }
        else
        {
            entry.offset = be32(bytes + numbers);
            entry.size = be32(bytes + numbers + 4);
        }
        if (!entry.key.empty())
        {
            out.push_back(std::move(entry));
        }
        at = numbers + number_size;
        const size_t percent = at * 90 / total;
        if (progress && percent != last_percent)
        {
            last_percent = percent;
            progress(static_cast<int>(percent));
        }
    }
    // The index is sorted its own way, so sort by the folded keys; the first of equal words stays first.
    std::stable_sort(out.begin(), out.end(), [](const Entry &a, const Entry &b) { return a.key < b.key; });
    if (progress)
    {
        progress(100);
    }
    return !out.empty();
}

Articles::Articles(Info info_in)
    : info(std::move(info_in))
{
    file.open(info.dict, std::ios::binary);
    if (!file)
    {
        return;
    }
    ready = info.compressed ? open_compressed() : true;
}

bool Articles::open_compressed()
{
    unsigned char head[12];
    file.read(reinterpret_cast<char *>(head), 12);
    if (file.gcount() != 12 || head[0] != 0x1f || head[1] != 0x8b || head[2] != 8 || !(head[3] & 4))
    {
        return false;
    }
    const uint32_t extra_length = head[10] | (head[11] << 8);
    std::string extra(extra_length, '\0');
    file.read(extra.data(), extra_length);
    if (static_cast<uint32_t>(file.gcount()) != extra_length)
    {
        return false;
    }
    const unsigned char *x = reinterpret_cast<const unsigned char *>(extra.data());
    bool found = false;
    for (size_t i = 0; i + 4 <= extra_length;)
    {
        const size_t length = x[i + 2] | (x[i + 3] << 8);
        if (x[i] == 'R' && x[i + 1] == 'A' && i + 4 + length <= extra_length && length >= 6)
        {
            const unsigned char *p = x + i + 4;
            chunk_length = p[2] | (p[3] << 8);
            const size_t count = p[4] | (p[5] << 8);
            if (length < 6 + count * 2 || chunk_length == 0)
            {
                return false;
            }
            for (size_t c = 0; c < count; ++c)
            {
                chunk_size.push_back(p[6 + c * 2] | (p[7 + c * 2] << 8));
            }
            found = true;
            break;
        }
        i += 4 + length;
    }
    if (!found)
    {
        return false;
    }
    uint64_t position = 12 + extra_length;
    int flags = head[3];
    char c;
    for (int mask : {0x08, 0x10})
    {
        if (flags & mask)
        {
            do
            {
                file.get(c);
                ++position;
            } while (file && c != '\0');
        }
    }
    if (flags & 0x02)
    {
        position += 2;
    }
    for (uint32_t size : chunk_size)
    {
        chunk_start.push_back(position);
        position += size;
    }
    return !chunk_size.empty();
}

bool Articles::read_raw(uint64_t offset, uint32_t size, std::string &out)
{
    out.clear();
    if (!info.compressed)
    {
        file.clear();
        file.seekg(static_cast<std::streamoff>(offset));
        out.resize(size);
        file.read(out.data(), size);
        out.resize(static_cast<size_t>(file.gcount()));
        return !out.empty();
    }
    uint64_t at = offset;
    const uint64_t end = offset + size;
    while (at < end)
    {
        const size_t chunk = static_cast<size_t>(at / chunk_length);
        if (chunk >= chunk_size.size())
        {
            break;
        }
        std::string packed(chunk_size[chunk], '\0');
        file.clear();
        file.seekg(static_cast<std::streamoff>(chunk_start[chunk]));
        file.read(packed.data(), static_cast<std::streamsize>(packed.size()));
        if (static_cast<size_t>(file.gcount()) != packed.size())
        {
            break;
        }
        // A chunk ends at a flush, not at a last block, so close it with an empty last block.
        packed.append("\x01\x00\x00\xFF\xFF", 5);
        int length = 0;
        char *plain = stbi_zlib_decode_noheader_malloc(packed.data(), static_cast<int>(packed.size()), &length);
        if (!plain)
        {
            break;
        }
        const uint64_t chunk_begin = static_cast<uint64_t>(chunk) * chunk_length;
        const uint64_t from = at - chunk_begin;
        const uint64_t take = std::min<uint64_t>(end - at, static_cast<uint64_t>(length) > from ? length - from : 0);
        if (take > 0)
        {
            out.append(plain + from, static_cast<size_t>(take));
        }
        std::free(plain);
        if (take == 0)
        {
            break;
        }
        at += take;
    }
    return !out.empty();
}

std::string Articles::text(uint64_t offset, uint32_t size)
{
    if (!ready || size == 0 || size > (16u << 20))
    {
        return std::string();
    }
    std::string raw;
    if (!read_raw(offset, size, raw))
    {
        return std::string();
    }
    return article_to_text(raw, info.types);
}

std::string article_to_text(const std::string &raw, const std::string &types)
{
    std::string out;
    const auto add = [&out](const std::string &part) {
        if (part.empty())
        {
            return;
        }
        if (!out.empty())
        {
            out.push_back('\n');
        }
        out += part;
    };
    if (!types.empty())
    {
        size_t at = 0;
        for (size_t i = 0; i < types.size() && at <= raw.size(); ++i)
        {
            const char type = types[i];
            const bool last = i + 1 == types.size();
            std::string data;
            if (last)
            {
                data = raw.substr(at);
                at = raw.size();
            }
            else if (std::islower(static_cast<unsigned char>(type)))
            {
                const size_t nul = raw.find('\0', at);
                data = raw.substr(at, nul == std::string::npos ? std::string::npos : nul - at);
                at = nul == std::string::npos ? raw.size() : nul + 1;
            }
            else
            {
                if (at + 4 > raw.size())
                {
                    break;
                }
                const uint32_t length = be32(reinterpret_cast<const unsigned char *>(raw.data()) + at);
                at += 4;
                data = raw.substr(at, length);
                at += std::min<size_t>(length, raw.size() - at);
            }
            if (!data.empty() && data.back() == '\0')
            {
                data.pop_back();
            }
            add(part_to_text(static_cast<char>(std::tolower(static_cast<unsigned char>(type))), data));
        }
    }
    else
    {
        // Every part starts with its type letter.
        size_t at = 0;
        while (at < raw.size())
        {
            const char type = raw[at++];
            std::string data;
            if (std::islower(static_cast<unsigned char>(type)))
            {
                const size_t nul = raw.find('\0', at);
                data = raw.substr(at, nul == std::string::npos ? std::string::npos : nul - at);
                at = nul == std::string::npos ? raw.size() : nul + 1;
            }
            else
            {
                if (at + 4 > raw.size())
                {
                    break;
                }
                const uint32_t length = be32(reinterpret_cast<const unsigned char *>(raw.data()) + at);
                at += 4;
                data = raw.substr(at, length);
                at += std::min<size_t>(length, raw.size() - at);
            }
            add(part_to_text(type, data));
        }
    }
    return tidy(out);
}

} // namespace stardict