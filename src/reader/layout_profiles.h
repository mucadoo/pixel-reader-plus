#ifndef LAYOUT_PROFILES_H_
#define LAYOUT_PROFILES_H_

#include <filesystem>
#include <string>
#include <vector>

class SystemStyling;
class TokenViewStyling;

// Named sets of the settings that shape the page: font, size, theme, margins, spacing, justify, indent,
// paragraph gap, bold, hyphenation and pictures. They are kept in one small file.
namespace layout_profiles
{

void load(std::filesystem::path file);

// The saved names, in the order they were first saved.
std::vector<std::string> names();

// Saves the current settings under the name, replacing a profile of that name.
void save(const std::string &name, const SystemStyling &system, const TokenViewStyling &styling);

// Puts a profile's settings in force. False when there is no such profile.
bool apply(const std::string &name, SystemStyling &system, TokenViewStyling &styling);

void remove(const std::string &name);

} // namespace layout_profiles

#endif