#include "./text_wrap.h"

#include "util/str_utils.h"
#include "util/utf8.h"

#include <cstring>
#include <string>
#include <stdexcept>
#include <vector>

namespace {

using str_filter_func = std::function<bool(const char *, uint32_t)>;

bool g_keep_punctuation = false;

// The text of the word that starts at the pointer, up to the next space or the end.
std::string word_at(const char *pos)
{
    const char *end = pos;
    while (*end && !is_whitespace(*end))
    {
        ++end;
    }
    return std::string(pos, end);
}

// The word that ends just before the space at pos, not reaching back before the start.
std::string word_before(const char *start, const char *pos)
{
    const char *begin = pos;
    while (begin > start && !is_whitespace(*(begin - 1)))
    {
        --begin;
    }
    return std::string(begin, pos);
}

bool is_number_word(const std::string &word)
{
    bool digit = false;
    for (const char c : word)
    {
        if (c >= '0' && c <= '9')
        {
            digit = true;
        }
        else if (c != '.' && c != ',' && c != '-')
        {
            return false;
        }
    }
    return digit;
}

bool is_unit_word(std::string word)
{
    while (!word.empty() && std::string(".,;:!?)").find(word.back()) != std::string::npos)
    {
        word.pop_back();
    }
    static const char *const units[] = {
        "km", "m", "cm", "mm", "kg", "g", "mg", "l", "ml", "h", "min", "s", "ms", "Hz", "kHz", "MHz",
        "km/h", "m/s", "%", "\xE2\x82\xAC", "$", "\xC2\xA3", "\xC2\xB0" "C", "\xC2\xB0" "F", "\xC2\xB0", "m\xC2\xB2", "km\xC2\xB2", "kg/m\xC2\xB3"
    };
    for (const char *unit : units)
    {
        if (word == unit)
        {
            return true;
        }
    }
    return false;
}

// Whether the line may be broken at the space at pos.
bool break_allowed(const char *start, const char *pos)
{
    if (!g_keep_punctuation || !*pos || *pos == '\n')
    {
        return true;
    }
    const char *next = pos + 1;
    const unsigned char first = static_cast<unsigned char>(*next);
    if (first == ':' || first == ';' || first == '!' || first == '?' || first == '%')
    {
        return false;
    }
    if (first == 0xC2 && static_cast<unsigned char>(next[1]) == 0xBB)
    {
        return false; // a closing guillemet
    }
    const std::string before = word_before(start, pos);
    if (before == "\xC2\xAB")
    {
        return false; // an opening guillemet
    }
    if (is_number_word(before) && is_unit_word(word_at(next)))
    {
        return false;
    }
    return true;
}

const char *find_first_break(const char *pos, int max_search)
{
    while (*pos && !is_whitespace(*pos)) {
        ++pos;
        if (--max_search < 0)
        {
            return nullptr;
        }
    }
    return pos;
}

const char *find_last_whitespace(const char *start_pos, str_filter_func &fits_on_line, uint32_t max_line_search_chars)
{
    const char *best_candidate_pos = nullptr;
    const char *kept_together_pos = nullptr; // a fitting place where a break is not wanted, used if there is no other
    const char *cur_pos = start_pos;

    while(true)
    {
        const char *candidate_pos = find_first_break(cur_pos, max_line_search_chars);
        if (!candidate_pos)
        {
            // no break within max search
            break;
        }

        if (!fits_on_line(start_pos, candidate_pos - start_pos))
        {
            // overshot
            break;
        }

        if (break_allowed(start_pos, candidate_pos))
        {
            best_candidate_pos = candidate_pos;
        }
        else
        {
            kept_together_pos = candidate_pos;
        }
        cur_pos = candidate_pos + 1;

        if (*candidate_pos == 0 || *candidate_pos == '\n')
        {
            // break on literal newlines and end of string
            break;
        }
    }

    return best_candidate_pos ? best_candidate_pos : kept_together_pos;
}

const char *find_last_character(const char *start_pos, str_filter_func &fits_on_line, uint32_t max_line_search_chars)
{
    const char *cur_pos = start_pos;
    while (*cur_pos && max_line_search_chars-- > 0)
    {
        const char *candidate_pos = utf8_step(cur_pos);
        if (!fits_on_line(start_pos, candidate_pos - start_pos))
        {
            break;
        }
        cur_pos = candidate_pos;
    }
    return cur_pos;
}

} // namespace

void set_keep_punctuation(bool on)
{
    g_keep_punctuation = on;
}

void wrap_lines_hyphenated(
    const char *str,
    std::function<bool(const char *, uint32_t)> fits_on_line,
    std::function<bool(const char *, uint32_t)> fits_with_hyphen,
    HyphenPointsFn hyphen_points,
    std::function<void(const char *, uint32_t, bool)> on_next_line,
    uint32_t max_line_search_chars
)
{
    uint32_t n = strlen(str);
    if (n == 0)
    {
        on_next_line(str, 0, false);
        return;
    }

    // The largest split of the word at word_start that leaves a first piece, with
    // the text before it on the line, that fits along with a hyphen.
    auto split_word = [&](const char *line_start, const char *word_start) -> const char * {
        if (!hyphen_points || !fits_with_hyphen)
        {
            return nullptr;
        }
        const char *word_end = find_first_break(word_start, static_cast<int>(max_line_search_chars));
        if (!word_end || word_end <= word_start)
        {
            return nullptr;
        }
        const std::vector<uint32_t> points = hyphen_points(word_start, static_cast<uint32_t>(word_end - word_start));
        for (auto it = points.rbegin(); it != points.rend(); ++it)
        {
            const char *at = word_start + *it;
            if (at > line_start && at < word_end && fits_with_hyphen(line_start, static_cast<uint32_t>(at - line_start)))
            {
                return at;
            }
        }
        return nullptr;
    };

    const char *cur_pos = str;
    const char *string_end_pos = cur_pos + n;
    while (cur_pos < string_end_pos)
    {
        const char *break_pos = find_last_whitespace(cur_pos, fits_on_line, max_line_search_chars);
        if (break_pos && (*break_pos == 0 || *break_pos == '\n'))
        {
            on_next_line(cur_pos, static_cast<uint32_t>(break_pos - cur_pos), false);
            cur_pos = break_pos + 1;
            continue;
        }

        // The next word did not fit. Maybe the front of it does.
        const char *next_word = break_pos ? break_pos + 1 : cur_pos;
        if (const char *split = split_word(cur_pos, next_word))
        {
            on_next_line(cur_pos, static_cast<uint32_t>(split - cur_pos), true);
            cur_pos = split;
            continue;
        }

        if (break_pos)
        {
            on_next_line(cur_pos, static_cast<uint32_t>(break_pos - cur_pos), false);
            cur_pos = break_pos + 1;
        }
        else if ((break_pos = find_last_character(cur_pos, fits_on_line, max_line_search_chars)))
        {
            on_next_line(cur_pos, static_cast<uint32_t>(break_pos - cur_pos), false);
            cur_pos = break_pos;
        }
        else
        {
            throw std::runtime_error("Failed to wrap line");
        }
    }
}
void wrap_lines(
    const char *str,
    std::function<bool(const char *, uint32_t)> fits_on_line,
    std::function<void(const char *, uint32_t)> on_next_line,
    uint32_t max_line_search_chars
)
{
    uint32_t n = strlen(str);
    if (n == 0)
    {
        on_next_line(str, 0);
        return;
    }

    const char *cur_pos = str;
    const char *string_end_pos = cur_pos + n;
    while (cur_pos < string_end_pos)
    {
        const char *break_pos;
        if ((break_pos = find_last_whitespace(cur_pos, fits_on_line, max_line_search_chars)))
        {
            on_next_line(cur_pos, break_pos - cur_pos);
            cur_pos = break_pos + 1;
        }
        else if ((break_pos = find_last_character(cur_pos, fits_on_line, max_line_search_chars)))
        {
            on_next_line(cur_pos, break_pos - cur_pos);
            cur_pos = break_pos;
        }
        else
        {
            throw std::runtime_error("Failed to wrap line");
        }
    }
}
