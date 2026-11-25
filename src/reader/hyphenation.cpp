#include "./hyphenation.h"

#include "./text_style.h"

#include <algorithm>
#include <fstream>

namespace
{

constexpr size_t MIN_BEFORE = 2; // letters kept on the first line
constexpr size_t MIN_AFTER = 3;  // letters carried to the next line

std::u32string decode(const std::string &text, std::vector<uint32_t> *offsets = nullptr)
{
    std::u32string out;
    for (size_t i = 0; i < text.size();)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        char32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0 && i + 3 < text.size())
        {
            cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12)
                | ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[i + 3]) & 0x3Fu);
            len = 4;
        }
        else if (c >= 0xE0 && i + 2 < text.size())
        {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6)
                | (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
            len = 3;
        }
        else if (c >= 0xC0 && i + 1 < text.size())
        {
            cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
            len = 2;
        }
        if (offsets)
        {
            offsets->push_back(static_cast<uint32_t>(i));
        }
        out.push_back(cp);
        i += len;
    }
    if (offsets)
    {
        offsets->push_back(static_cast<uint32_t>(text.size()));
    }
    return out;
}

bool is_letter(char32_t cp)
{
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= 0xC0 && cp <= 0x24F && cp != 0xD7 && cp != 0xF7);
}

} // namespace

bool Hyphenator::load(const std::string &name)
{
    patterns.clear();
    exceptions.clear();
    cache.clear();
    longest = 0;
    loaded_ok = false;

    const std::string candidates[] = {
        "resources/hyphenation/" + name + ".txt",
        "/mnt/SDCARD/App/PixelReader/resources/hyphenation/" + name + ".txt",
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
        return false;
    }

    std::string line;
    while (std::getline(in, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        {
            line.pop_back();
        }
        if (line.empty() || line[0] == '%')
        {
            continue;
        }
        if (line.find('-') != std::string::npos)
        {
            // An exception word: the hyphens are the allowed breaks.
            std::u32string letters;
            std::vector<uint32_t> breaks;
            for (char32_t cp : decode(lookup_lowercase(line)))
            {
                if (cp == U'-')
                {
                    breaks.push_back(static_cast<uint32_t>(letters.size()));
                }
                else
                {
                    letters.push_back(cp);
                }
            }
            exceptions[letters] = breaks;
            continue;
        }
        std::u32string letters;
        std::vector<uint8_t> values(1, 0);
        for (char32_t cp : decode(line))
        {
            if (cp >= U'0' && cp <= U'9')
            {
                values.back() = static_cast<uint8_t>(cp - U'0');
            }
            else
            {
                letters.push_back(cp);
                values.push_back(0);
            }
        }
        if (!letters.empty())
        {
            longest = std::max(longest, letters.size());
            patterns[letters] = values;
        }
    }
    loaded_ok = !patterns.empty();
    return loaded_ok;
}

std::vector<uint32_t> Hyphenator::points(const std::string &word) const
{
    if (!loaded_ok || word.size() < MIN_BEFORE + MIN_AFTER)
    {
        return {};
    }
    const auto cached = cache.find(word);
    if (cached != cache.end())
    {
        return cached->second;
    }

    std::vector<uint32_t> offsets;
    const std::u32string original = decode(word, &offsets);
    std::vector<uint32_t> result;
    bool plain = original.size() >= MIN_BEFORE + MIN_AFTER;
    for (char32_t cp : original)
    {
        plain = plain && is_letter(cp);
    }
    if (plain)
    {
        const std::u32string letters = decode(lookup_lowercase(word));
        std::vector<uint32_t> breaks; // after this many letters
        const auto exception = exceptions.find(letters);
        if (exception != exceptions.end())
        {
            breaks = exception->second;
        }
        else
        {
            const std::u32string padded = U"." + letters + U".";
            std::vector<uint8_t> score(padded.size() + 1, 0);
            for (size_t start = 0; start < padded.size(); ++start)
            {
                for (size_t len = 1; len <= longest && start + len <= padded.size(); ++len)
                {
                    const auto hit = patterns.find(padded.substr(start, len));
                    if (hit == patterns.end())
                    {
                        continue;
                    }
                    for (size_t k = 0; k < hit->second.size(); ++k)
                    {
                        score[start + k] = std::max(score[start + k], hit->second[k]);
                    }
                }
            }
            // score[i + 1] sits between padded[i] and padded[i + 1]; padded[0] is the dot.
            for (size_t after = 1; after < letters.size(); ++after)
            {
                if (score[after + 1] % 2 == 1)
                {
                    breaks.push_back(static_cast<uint32_t>(after));
                }
            }
        }
        for (uint32_t after : breaks)
        {
            if (after >= MIN_BEFORE && letters.size() - after >= MIN_AFTER && after < offsets.size())
            {
                result.push_back(offsets[after]);
            }
        }
    }
    cache.emplace(word, result);
    return result;
}
