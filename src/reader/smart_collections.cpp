#include "./smart_collections.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <system_error>

namespace smart_collections
{

namespace
{

std::filesystem::path store_file;
std::map<std::string, std::vector<Rule>> shelves;
bool dirty = false;

const char *KEYS[] = {"author", "tag", "series", "language", "publisher", "year", "status", "type", "rating"};

std::string lower(std::string text)
{
    for (char &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string trimmed(const std::string &text)
{
    size_t a = 0;
    size_t b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a])))
    {
        ++a;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1])))
    {
        --b;
    }
    return text.substr(a, b - a);
}

bool contains(const std::string &haystack, const std::string &needle)
{
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

int to_int(const std::string &text, bool &ok)
{
    ok = !text.empty() && text.size() < 6;
    int value = 0;
    for (char c : text)
    {
        if (c < '0' || c > '9')
        {
            ok = false;
            return 0;
        }
        value = value * 10 + (c - '0');
    }
    return value;
}

// 1984 or 1980-1989 against a year.
bool year_in(const std::string &want, const std::string &year)
{
    bool ok = false;
    const int have = to_int(year, ok);
    if (!ok)
    {
        return false;
    }
    const size_t dash = want.find('-');
    if (dash == std::string::npos)
    {
        bool ok_one = false;
        const int one = to_int(trimmed(want), ok_one);
        return ok_one && one == have;
    }
    bool ok_a = false;
    bool ok_b = false;
    const int a = to_int(trimmed(want.substr(0, dash)), ok_a);
    const int b = to_int(trimmed(want.substr(dash + 1)), ok_b);
    return ok_a && ok_b && have >= std::min(a, b) && have <= std::max(a, b);
}

bool rule_holds(const Rule &rule, const Facts &facts)
{
    std::string want = trimmed(rule.value);
    bool negate = false;
    if (!want.empty() && want[0] == '!')
    {
        negate = true;
        want = trimmed(want.substr(1));
    }
    if (want.empty())
    {
        return true; // nothing to look for does not exclude anything
    }
    bool holds = false;
    switch (rule.field)
    {
        case Field::Author: holds = contains(facts.author, want); break;
        case Field::Tag: holds = contains(facts.tags, want); break;
        case Field::Series: holds = contains(facts.series, want); break;
        case Field::Language: holds = contains(facts.language, want); break;
        case Field::Publisher: holds = contains(facts.publisher, want); break;
        case Field::Year: holds = year_in(want, facts.year); break;
        case Field::Type: holds = lower(facts.type) == lower(want); break;
        case Field::Status:
            holds = lower(want) == "favorite" ? facts.favorite : lower(facts.status) == lower(want);
            break;
        case Field::Rating:
        {
            bool ok = false;
            const int stars = to_int(want, ok);
            holds = ok && facts.rating >= stars && facts.rating > 0;
            break;
        }
        default: break;
    }
    return negate ? !holds : holds;
}

} // namespace

const char *field_name(Field field)
{
    static const char *NAMES[] = {"Author", "Tag", "Series", "Language", "Publisher", "Year", "Status", "Type", "Rating"};
    const size_t index = static_cast<size_t>(field);
    return index < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[index] : "";
}

std::string describe(const Rule &rule)
{
    std::string want = trimmed(rule.value);
    bool negate = false;
    if (!want.empty() && want[0] == '!')
    {
        negate = true;
        want = trimmed(want.substr(1));
    }
    std::string verb;
    switch (rule.field)
    {
        case Field::Status:
        case Field::Type:
            verb = negate ? " is not " : " is ";
            break;
        case Field::Year:
            verb = negate ? " is not " : " is ";
            break;
        case Field::Rating:
            return std::string("Rating ") + (negate ? "under " : "at least ") + want + (want == "1" ? " star" : " stars");
        default:
            verb = negate ? " does not contain " : " contains ";
            break;
    }
    return std::string(field_name(rule.field)) + verb + want;
}

std::string describe(const std::vector<Rule> &rules)
{
    std::string out;
    for (const Rule &rule : rules)
    {
        out += (out.empty() ? "" : "; ") + describe(rule);
    }
    return out;
}

bool matches(const std::vector<Rule> &rules, const Facts &facts)
{
    if (rules.empty())
    {
        return false;
    }
    for (const Rule &rule : rules)
    {
        if (!rule_holds(rule, facts))
        {
            return false;
        }
    }
    return true;
}

void load(std::filesystem::path file)
{
    store_file = std::move(file);
    shelves.clear();
    std::ifstream in(store_file);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t first = line.find('\t');
        const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos || first == 0)
        {
            continue;
        }
        const std::string name = line.substr(0, first);
        const std::string key = line.substr(first + 1, second - first - 1);
        const std::string value = line.substr(second + 1);
        for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); ++i)
        {
            if (key == KEYS[i])
            {
                shelves[name].push_back({static_cast<Field>(i), value});
                break;
            }
        }
    }
    dirty = false;
}

void flush()
{
    if (!dirty || store_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(store_file.parent_path(), ec);
    std::ofstream out(store_file, std::ios::trunc);
    for (const auto &shelf : shelves)
    {
        for (const Rule &rule : shelf.second)
        {
            std::string value = rule.value;
            std::replace(value.begin(), value.end(), '\t', ' ');
            std::replace(value.begin(), value.end(), '\n', ' ');
            out << shelf.first << '\t' << KEYS[static_cast<size_t>(rule.field)] << '\t' << value << '\n';
        }
    }
    dirty = false;
}

std::vector<std::string> names()
{
    std::vector<std::string> out;
    for (const auto &shelf : shelves)
    {
        out.push_back(shelf.first);
    }
    return out;
}

std::vector<Rule> rules(const std::string &name)
{
    const auto found = shelves.find(name);
    return found == shelves.end() ? std::vector<Rule>() : found->second;
}

void set(const std::string &name, std::vector<Rule> rules)
{
    if (name.empty() || rules.empty())
    {
        return;
    }
    shelves[name] = std::move(rules);
    dirty = true;
}

void remove(const std::string &name)
{
    if (shelves.erase(name))
    {
        dirty = true;
    }
}

} // namespace smart_collections
