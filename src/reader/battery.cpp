#include "./battery.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

// The folder of the battery in the power supply list, or empty when there is none.
std::filesystem::path battery_dir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator("/sys/class/power_supply", ec))
    {
        std::ifstream type(entry.path() / "type");
        std::string kind;
        if (type >> kind && kind == "Battery")
        {
            return entry.path();
        }
    }
    return {};
}

} // namespace

int battery_percent()
{
    const auto dir = battery_dir();
    if (dir.empty())
    {
        return -1;
    }
    std::ifstream capacity(dir / "capacity");
    int level = -1;
    if (capacity >> level && level >= 0 && level <= 100)
    {
        return level;
    }
    return -1;
}

bool battery_charging()
{
    const auto dir = battery_dir();
    if (dir.empty())
    {
        return false;
    }
    std::ifstream status(dir / "status");
    std::string text;
    status >> text;
    return text == "Charging" || text == "Full";
}