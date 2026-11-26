#ifndef SETTINGS_MENUS_H_
#define SETTINGS_MENUS_H_

class SystemStyling;
class TokenViewStyling;
class ViewStack;

// The style tweaks of a book: switches kept in the settings.
void open_style_tweaks_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling);

// Ligatures, and keeping one line of a paragraph from standing alone at the top or bottom of a page.
void open_typography_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling);

// Pick the background, text and highlight colors of the theme called Custom, or start from a preset.
void open_custom_colors_menu(ViewStack &stack, SystemStyling &system);

// Saved sets of page settings: save the current ones, use one, replace it, delete it.
void open_layout_profiles_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling);

// The order the dictionaries are listed and used in, and which ones are hidden.
void open_dictionary_order_menu(ViewStack &stack, SystemStyling &system);

#endif