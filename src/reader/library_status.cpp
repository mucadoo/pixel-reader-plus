#include "./library_status.h"

#include "./battery.h"

#include <cstdio>
#include <ctime>
#include <system_error>

namespace library_status
{

namespace
{

int shown_level = 0;
bool twelve_hours = false;
std::filesystem::path folder;

std::string free_space_text()
{
    std::error_code ec;
    const auto info = std::filesystem::space(folder.empty() ? std::filesystem::path("/") : folder, ec);
    if (ec)
    {
        return "";
    }
    const double gigabytes = static_cast<double>(info.available) / (1024.0 * 1024.0 * 1024.0);
    char text[32];
    if (gigabytes >= 10)
    {
        std::snprintf(text, sizeof(text), "%.0f GB free", gigabytes);
    }
    else if (gigabytes >= 1)
    {
        std::snprintf(text, sizeof(text), "%.1f GB free", gigabytes);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%.0f MB free", gigabytes * 1024.0);
    }
    return text;
}

} // namespace

void configure(int level, bool clock_12_hours)
{
    shown_level = level;
    twelve_hours = clock_12_hours;
}

int level()
{
    return shown_level;
}

void set_folder(const std::filesystem::path &new_folder)
{
    folder = new_folder;
}

std::string text()
{
    if (shown_level <= 0)
    {
        return "";
    }
    std::string line;
    const std::time_t now = std::time(nullptr);
    std::tm local {};
    if (localtime_r(&now, &local))
    {
        char clock[24];
        std::strftime(clock, sizeof(clock), twelve_hours ? "%I:%M %p" : "%H:%M", &local);
        line = (twelve_hours && clock[0] == '0') ? std::string(clock + 1) : std::string(clock);
    }
    const int charge = battery_percent();
    if (charge >= 0)
    {
        line += std::string(line.empty() ? "" : "  ") + std::to_string(charge) + "%" + (battery_charging() ? "+" : "");
    }
    if (shown_level >= 2)
    {
        const std::string space = free_space_text();
        if (!space.empty())
        {
            line += std::string(line.empty() ? "" : "  ") + space;
        }
    }
    return line;
}

} // namespace library_status