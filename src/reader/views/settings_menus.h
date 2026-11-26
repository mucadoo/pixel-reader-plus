#ifndef SETTINGS_MENUS_H_
#define SETTINGS_MENUS_H_

class SystemStyling;
class TokenViewStyling;
class ViewStack;

// The style tweaks of a book: switches kept in the settings.
void open_style_tweaks_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling);

// Saved sets of page settings: save the current ones, use one, replace it, delete it.
void open_layout_profiles_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling);

#endif