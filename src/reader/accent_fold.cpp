#include "./accent_fold.h"

namespace
{

struct Range
{
    uint32_t first;
    uint32_t last;
    char base; // the capital; the second of each pair is the small letter
    bool paired; // false: every letter of the range is the same case
};

const Range RANGES[] = {
    {0x00C0, 0x00C5, 'A', false}, {0x00C7, 0x00C7, 'C', false}, {0x00C8, 0x00CB, 'E', false},
    {0x00CC, 0x00CF, 'I', false}, {0x00D0, 0x00D0, 'D', false}, {0x00D1, 0x00D1, 'N', false},
    {0x00D2, 0x00D6, 'O', false}, {0x00D8, 0x00D8, 'O', false}, {0x00D9, 0x00DC, 'U', false},
    {0x00DD, 0x00DD, 'Y', false},
    {0x0100, 0x0105, 'A', true}, {0x0106, 0x010D, 'C', true}, {0x010E, 0x0111, 'D', true},
    {0x0112, 0x011B, 'E', true}, {0x011C, 0x0123, 'G', true}, {0x0124, 0x0127, 'H', true},
    {0x0128, 0x0131, 'I', true}, {0x0134, 0x0135, 'J', true}, {0x0136, 0x0137, 'K', true},
    {0x0139, 0x0142, 'L', true}, {0x0143, 0x0148, 'N', true}, {0x014C, 0x0151, 'O', true},
    {0x0154, 0x0159, 'R', true}, {0x015A, 0x0161, 'S', true}, {0x0162, 0x0167, 'T', true},
    {0x0168, 0x0173, 'U', true}, {0x0174, 0x0175, 'W', true}, {0x0176, 0x0177, 'Y', true},
    {0x0179, 0x017E, 'Z', true},
};

// The small-letter ranges of Latin-1, which follow the capitals 32 places on.
const Range SMALL[] = {
    {0x00E0, 0x00E5, 'a', false}, {0x00E7, 0x00E7, 'c', false}, {0x00E8, 0x00EB, 'e', false},
    {0x00EC, 0x00EF, 'i', false}, {0x00F0, 0x00F0, 'd', false}, {0x00F1, 0x00F1, 'n', false},
    {0x00F2, 0x00F6, 'o', false}, {0x00F8, 0x00F8, 'o', false}, {0x00F9, 0x00FC, 'u', false},
    {0x00FD, 0x00FD, 'y', false}, {0x00FF, 0x00FF, 'y', false},
};

// What the letter becomes, or null when it stays as it is.
const char *plain_for(uint32_t cp, char *one)
{
    switch (cp)
    {
        case 0x00C6: return "AE";
        case 0x00E6: return "ae";
        case 0x00DF: return "ss";
        case 0x0152: return "OE";
        case 0x0153: return "oe";
        case 0x0178: return "Y";
        case 0x017F: return "s";
        default: break;
    }
    for (const Range &range : RANGES)
    {
        if (cp >= range.first && cp <= range.last)
        {
            const bool small = range.paired && ((cp - range.first) % 2 == 1);
            one[0] = small ? static_cast<char>(range.base + 32) : range.base;
            one[1] = 0;
            return one;
        }
    }
    for (const Range &range : SMALL)
    {
        if (cp >= range.first && cp <= range.last)
        {
            one[0] = range.base;
            one[1] = 0;
            return one;
        }
    }
    return nullptr;
}

} // namespace

std::string fold_accents(const std::string &text, std::vector<uint32_t> *map)
{
    std::string out;
    out.reserve(text.size());
    if (map)
    {
        map->clear();
        map->reserve(text.size());
    }
    const size_t n = text.size();
    size_t i = 0;
    while (i < n)
    {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        size_t length = 1;
        uint32_t cp = lead;
        if (lead >= 0xC0 && lead < 0xE0 && i + 1 < n)
        {
            length = 2;
            cp = ((lead & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
        }
        else if (lead >= 0xE0 && lead < 0xF0 && i + 2 < n)
        {
            length = 3;
            cp = ((lead & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
        }
        else if (lead >= 0xF0 && i + 3 < n)
        {
            length = 4;
        }
        char one[2] = {0, 0};
        const char *plain = length == 2 ? plain_for(cp, one) : nullptr;
        if (plain)
        {
            for (const char *p = plain; *p; ++p)
            {
                out.push_back(*p);
                if (map)
                {
                    map->push_back(static_cast<uint32_t>(i));
                }
            }
        }
        else
        {
            for (size_t k = 0; k < length; ++k)
            {
                out.push_back(text[i + k]);
                if (map)
                {
                    map->push_back(static_cast<uint32_t>(i));
                }
            }
        }
        i += length;
    }
    return out;
}