#ifndef COLOR_THEME_DEF_H_
#define COLOR_THEME_DEF_H_

#include "./color_theme.h"
#include <string>

const ColorTheme& get_color_theme(const std::string &name);
int theme_count();
std::string theme_name_at(int index);
std::string get_valid_theme(const std::string &preferred);
std::string get_prev_theme(const std::string &name);
std::string get_next_theme(const std::string &name);

// The theme called "custom": colors the user picked. Its other colors follow from these three.
void set_custom_theme(SDL_Color background, SDL_Color text, SDL_Color highlight);
void get_custom_theme(SDL_Color &background, SDL_Color &text, SDL_Color &highlight);
// True once colors were set, from the settings or the Custom colors menu.
bool custom_theme_chosen();

#endif
