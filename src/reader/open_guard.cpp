#include "./open_guard.h"

#include <fstream>
#include <string>
#include <system_error>

namespace open_guard
{

namespace
{

std::filesystem::path guard_file;
std::filesystem::path plain_book;

constexpr long long QUICK_SECONDS = 90;

void write_state(const char *state, long long now, const std::filesystem::path &book)
{
    if (guard_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(guard_file.parent_path(), ec);
    std::ofstream out(guard_file, std::ios::trunc);
    out << state << '\t' << now << '\t' << book.string() << '\n';
}

std::filesystem::path current_book;

} // namespace

void set_file(std::filesystem::path file)
{
    guard_file = std::move(file);
}

void begin(const std::filesystem::path &book, long long now)
{
    current_book = book;
    write_state("opening", now, book);
}

void opened(long long now)
{
    if (!current_book.empty())
    {
        write_state("opened", now, current_book);
    }
}

void end()
{
    current_book.clear();
    if (guard_file.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(guard_file, ec);
}

std::optional<Crash> take_crash(long long now)
{
    if (guard_file.empty())
    {
        return std::nullopt;
    }
    std::string line;
    {
        std::ifstream in(guard_file);
        if (!in || !std::getline(in, line))
        {
            std::error_code ec;
            std::filesystem::remove(guard_file, ec);
            return std::nullopt;
        }
    }
    std::error_code ec;
    std::filesystem::remove(guard_file, ec);
    const size_t first = line.find('\t');
    const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
    if (second == std::string::npos)
    {
        return std::nullopt;
    }
    const std::string state = line.substr(0, first);
    long long when = 0;
    try
    {
        when = std::stoll(line.substr(first + 1, second - first - 1));
    }
    catch (...)
    {
        return std::nullopt;
    }
    Crash crash;
    crash.book = line.substr(second + 1);
    if (crash.book.empty())
    {
        return std::nullopt;
    }
    if (state == "opening")
    {
        crash.while_opening = true;
        return crash;
    }
    if (state == "opened" && now - when >= 0 && now - when < QUICK_SECONDS)
    {
        return crash;
    }
    return std::nullopt;
}

void plain_next(const std::filesystem::path &book)
{
    plain_book = book;
}

bool take_plain(const std::filesystem::path &book)
{
    if (!plain_book.empty() && plain_book == book)
    {
        plain_book.clear();
        return true;
    }
    return false;
}

} // namespace open_guard
