#include "./settings_menus.h"

#include "./selection_menu.h"
#include "./text_input_view.h"
#include "filetypes/epub/book_style.h"
#include "reader/button_actions.h"
#include "reader/dictionary.h"
#include "reader/color_theme_def.h"
#include "reader/layout_profiles.h"
#include "reader/system_styling.h"
#include "reader/view_stack.h"
#include "reader/views/token_view/token_view_styling.h"

#include "sys/keymap.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{

struct Tweak
{
    int bit;
    const char *name;
};

const Tweak TWEAKS[] = {
    {TWEAK_PLAIN_BREAKS, "Ignore page breaks"},
    {TWEAK_PLAIN_SIZES, "Ignore font sizes"},
    {TWEAK_PLAIN_CENTER, "Ignore centered text"},
    {TWEAK_SMART_QUOTES, "Curly quotes"},
    {TWEAK_PLAIN_FONTS, "Ignore the book's font"},
    {TWEAK_PLAIN_PAGES, "Ignore the book's page numbers"},
};

std::vector<std::string> tweak_rows(int mask)
{
    std::vector<std::string> rows;
    for (const Tweak &tweak : TWEAKS)
    {
        rows.push_back(std::string(tweak.name));
    }
    (void)mask;
    return rows;
}

std::vector<std::string> tweak_details(int mask)
{
    std::vector<std::string> details;
    for (const Tweak &tweak : TWEAKS)
    {
        details.push_back((mask & tweak.bit) ? "On" : "Off");
    }
    return details;
}

struct NamedColor
{
    const char *name;
    SDL_Color color;
};

const NamedColor PALETTE[] = {
    {"Black", {0, 0, 0, 0}},
    {"Charcoal", {40, 40, 40, 0}},
    {"Dark gray", {80, 80, 80, 0}},
    {"Gray", {128, 128, 128, 0}},
    {"Light gray", {200, 200, 200, 0}},
    {"White", {255, 255, 255, 0}},
    {"Cream", {250, 240, 220, 0}},
    {"Paper", {238, 228, 200, 0}},
    {"Warm text", {235, 219, 178, 0}},
    {"Dark brown", {45, 32, 20, 0}},
    {"Navy", {16, 24, 48, 0}},
    {"Deep teal", {0, 43, 54, 0}},
    {"Dark green", {18, 40, 28, 0}},
    {"Red", {192, 0, 0, 0}},
    {"Orange", {230, 130, 40, 0}},
    {"Amber", {255, 176, 0, 0}},
    {"Yellow", {255, 224, 120, 0}},
    {"Green", {90, 170, 90, 0}},
    {"Teal", {38, 160, 150, 0}},
    {"Blue", {70, 130, 200, 0}},
    {"Purple", {150, 38, 200, 0}},
    {"Pink", {240, 150, 190, 0}},
};
constexpr int PALETTE_COUNT = static_cast<int>(sizeof(PALETTE) / sizeof(PALETTE[0]));

struct ColorPreset
{
    const char *name;
    SDL_Color background;
    SDL_Color text;
    SDL_Color highlight;
};

const ColorPreset PRESETS[] = {
    {"Solarized light", {253, 246, 227, 0}, {88, 110, 117, 0}, {38, 139, 210, 0}},
    {"Solarized dark", {0, 43, 54, 0}, {147, 161, 161, 0}, {181, 137, 0, 0}},
    {"Paper", {238, 228, 200, 0}, {45, 32, 20, 0}, {163, 81, 200, 0}},
    {"Amber terminal", {0, 0, 0, 0}, {255, 176, 0, 0}, {255, 176, 0, 0}},
    {"Green terminal", {0, 0, 0, 0}, {90, 220, 90, 0}, {90, 220, 90, 0}},
    {"Blue night", {16, 24, 48, 0}, {200, 215, 240, 0}, {70, 130, 200, 0}},
};
constexpr int PRESET_COUNT = static_cast<int>(sizeof(PRESETS) / sizeof(PRESETS[0]));

bool same_color(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int palette_index(SDL_Color color)
{
    for (int i = 0; i < PALETTE_COUNT; ++i)
    {
        if (same_color(PALETTE[i].color, color))
        {
            return i;
        }
    }
    return -1;
}

std::string color_name(SDL_Color color)
{
    const int index = palette_index(color);
    if (index >= 0)
    {
        return PALETTE[index].name;
    }
    char text[16];
    snprintf(text, sizeof(text), "#%02X%02X%02X", color.r, color.g, color.b);
    return text;
}

void show_custom_colors(SystemStyling &system, SDL_Color background, SDL_Color text, SDL_Color highlight)
{
    set_custom_theme(background, text, highlight);
    if (system.get_color_theme() == "custom")
    {
        system.reload_color_theme();
    }
    else
    {
        system.set_color_theme("custom");
    }
}

std::vector<std::string> custom_color_details()
{
    SDL_Color background, text, highlight;
    get_custom_theme(background, text, highlight);
    return {color_name(background), color_name(text), color_name(highlight), "Next"};
}

} // namespace

void open_custom_colors_menu(ViewStack &stack, SystemStyling &system)
{
    if (!custom_theme_chosen())
    {
        // Start from the colors in use, so what you see is what you change.
        const ColorTheme &now = system.get_loaded_color_theme();
        set_custom_theme(now.background, now.main_text, now.highlight_background);
    }
    auto menu = std::make_shared<SelectionMenu>(
        std::vector<std::string> {"Background", "Text", "Highlight", "Preset"},
        system
    );
    menu->set_details(custom_color_details());
    menu->set_title("Custom colors");
    menu->set_compact(true);
    menu->set_hints({"A  Next color", "B  Close"});
    std::weak_ptr<SelectionMenu> weak = menu;
    auto next_preset = std::make_shared<int>(0);
    menu->set_on_selection([weak, &system, next_preset](uint32_t index) {
        SDL_Color background, text, highlight;
        get_custom_theme(background, text, highlight);
        if (index < 3)
        {
            SDL_Color &chosen = index == 0 ? background : (index == 1 ? text : highlight);
            const int at = palette_index(chosen);
            chosen = PALETTE[(at + 1) % PALETTE_COUNT].color;
        }
        else if (index == 3)
        {
            const ColorPreset &preset = PRESETS[*next_preset % PRESET_COUNT];
            background = preset.background;
            text = preset.text;
            highlight = preset.highlight;
            *next_preset = (*next_preset + 1) % PRESET_COUNT;
        }
        show_custom_colors(system, background, text, highlight);
        if (auto self = weak.lock())
        {
            auto details = custom_color_details();
            if (index == 3)
            {
                details[3] = std::string(PRESETS[(*next_preset + PRESET_COUNT - 1) % PRESET_COUNT].name);
            }
            self->set_details(details);
        }
    });
    stack.push(menu);
}

namespace
{

std::vector<std::string> typography_details(const TokenViewStyling &styling)
{
    return {
        styling.get_ligatures() ? "On" : "Off",
        styling.get_widow_control() ? "On" : "Off",
    };
}

} // namespace

void open_dictionary_order_menu(ViewStack &stack, SystemStyling &system)
{
    // The names in the order shown, and the hidden ones; changes are saved at once.
    auto order = std::make_shared<std::vector<std::string>>(dictionary_names_in_order());
    auto hidden = std::make_shared<std::set<std::string>>();
    for (const std::string &name : *order)
    {
        if (dictionary_hidden(name))
        {
            hidden->insert(name);
        }
    }
    const auto fill = [order, hidden](SelectionMenu &m) {
        std::vector<std::string> details;
        int place = 0;
        for (const std::string &name : *order)
        {
            details.push_back(hidden->count(name) ? std::string("hidden") : std::to_string(++place));
        }
        m.set_entries(*order);
        m.set_details(details);
    };
    auto menu = std::make_shared<SelectionMenu>(system);
    fill(*menu);
    menu->set_title("Dictionaries (first is tried first)");
    menu->set_empty_message("No dictionaries on the card");
    menu->set_hints({"A  Up", "Y  Down", "X  Hide / show", "B  Close"});
    menu->set_uses_x(true);
    menu->set_default_on_keypress([order, hidden, fill](SDLKey key, SelectionMenu &m) {
        const uint32_t at = m.get_cursor_pos();
        if (at >= order->size())
        {
            return;
        }
        if (key == SW_BTN_Y && at + 1 < order->size())
        {
            std::swap((*order)[at], (*order)[at + 1]);
            fill(m);
            m.set_cursor_pos(at + 1);
        }
        else if (key == SW_BTN_X)
        {
            const std::string &name = (*order)[at];
            if (hidden->count(name))
            {
                hidden->erase(name);
            }
            else if (hidden->size() + 1 < order->size())
            {
                hidden->insert(name);
            }
            fill(m);
        }
        else
        {
            return;
        }
        set_dictionary_preferences(*order, *hidden);
    });
    menu->set_on_selection([order, hidden, fill, weak = std::weak_ptr<SelectionMenu>(menu)](uint32_t at) {
        // A moves the dictionary up one place.
        if (at > 0 && at < order->size())
        {
            std::swap((*order)[at], (*order)[at - 1]);
            if (auto m = weak.lock())
            {
                fill(*m);
                m->set_cursor_pos(at - 1);
            }
            set_dictionary_preferences(*order, *hidden);
        }
    });
    stack.push(menu);
}

void open_button_actions_menu(ViewStack &stack, SystemStyling &system)
{
    const auto current = []() {
        std::vector<std::string> names;
        for (int slot = 0; slot < button_actions::SLOT_COUNT; ++slot)
        {
            names.push_back(button_actions::action_name(button_actions::get(static_cast<button_actions::Slot>(slot))));
        }
        return names;
    };
    std::vector<std::string> slot_names;
    for (int slot = 0; slot < button_actions::SLOT_COUNT; ++slot)
    {
        slot_names.push_back(button_actions::slot_name(static_cast<button_actions::Slot>(slot)));
    }
    auto menu = std::make_shared<SelectionMenu>(slot_names, system);
    menu->set_details(current());
    menu->set_title("Button actions while reading");
    menu->set_compact(true);
    menu->set_hints({"A  Change", "B  Close"});
    std::weak_ptr<SelectionMenu> weak = menu;
    menu->set_on_selection([weak, &stack, &system, current](uint32_t index) {
        if (index >= button_actions::SLOT_COUNT)
        {
            return;
        }
        const auto slot = static_cast<button_actions::Slot>(index);
        std::vector<std::string> names;
        for (int action = 0; action < button_actions::ACTION_COUNT; ++action)
        {
            names.push_back(button_actions::action_name(action));
        }
        auto choices = std::make_shared<SelectionMenu>(names, system);
        choices->set_title(button_actions::slot_name(slot));
        choices->set_compact(true);
        choices->set_close_on_select();
        choices->set_marker(static_cast<uint32_t>(button_actions::get(slot)));
        choices->set_cursor_pos(static_cast<uint32_t>(button_actions::get(slot)));
        choices->set_hints({"A  Use", "B  Back"});
        choices->set_on_selection([weak, slot, current](uint32_t action) {
            button_actions::set(slot, static_cast<int>(action));
            if (auto self = weak.lock())
            {
                self->set_details(current());
            }
        });
        stack.push(choices);
    });
    stack.push(menu);
}

void open_typography_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling)
{
    auto menu = std::make_shared<SelectionMenu>(
        std::vector<std::string> {"Ligatures (fi, fl, ff)", "Widows and orphans (page mode)"},
        system
    );
    menu->set_details(typography_details(styling));
    menu->set_title("Typography (for this book)");
    menu->set_compact(true);
    menu->set_hints({"A  Switch", "B  Close"});
    std::weak_ptr<SelectionMenu> weak = menu;
    menu->set_on_selection([weak, &styling](uint32_t index) {
        if (index == 0)
        {
            styling.set_ligatures(!styling.get_ligatures());
        }
        else if (index == 1)
        {
            styling.set_widow_control(!styling.get_widow_control());
        }
        if (auto self = weak.lock())
        {
            self->set_details(typography_details(styling));
        }
    });
    stack.push(menu);
}

void open_style_tweaks_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling)
{
    auto menu = std::make_shared<SelectionMenu>(tweak_rows(styling.get_tweaks()), system);
    menu->set_details(tweak_details(styling.get_tweaks()));
    menu->set_title("Style tweaks (from the next book you open)");
    menu->set_compact(true);
    menu->set_hints({"A  Switch", "B  Close"});
    std::weak_ptr<SelectionMenu> weak = menu;
    menu->set_on_selection([weak, &styling](uint32_t index) {
        if (index >= sizeof(TWEAKS) / sizeof(TWEAKS[0]))
        {
            return;
        }
        styling.set_tweaks(styling.get_tweaks() ^ TWEAKS[index].bit);
        if (auto self = weak.lock())
        {
            self->set_details(tweak_details(styling.get_tweaks()));
        }
    });
    stack.push(menu);
}

namespace
{

void open_profile_actions(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling, const std::string &name)
{
    auto menu = std::make_shared<SelectionMenu>(
        std::vector<std::string> {"Use this layout", "Replace it with the current settings", "Delete it"},
        system
    );
    menu->set_title(name);
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&stack, &system, &styling, name](uint32_t index) {
        if (index == 0)
        {
            layout_profiles::apply(name, system, styling);
        }
        else if (index == 1)
        {
            layout_profiles::save(name, system, styling);
        }
        else if (index == 2)
        {
            layout_profiles::remove(name);
        }
    });
    stack.push(menu);
}

} // namespace

void open_layout_profiles_menu(ViewStack &stack, SystemStyling &system, TokenViewStyling &styling)
{
    std::vector<std::string> rows = {"Save the current settings as..."};
    const std::vector<std::string> saved = layout_profiles::names();
    for (const std::string &name : saved)
    {
        rows.push_back(name);
    }
    auto menu = std::make_shared<SelectionMenu>(rows, system);
    menu->set_title("Layout profiles");
    menu->set_compact(true);
    menu->set_close_on_select();
    menu->set_on_selection([&stack, &system, &styling, saved](uint32_t index) {
        if (index == 0)
        {
            stack.push(std::make_shared<TextInputView>(
                "Name of this layout",
                "",
                [&stack, &system, &styling](const std::string &typed) {
                    if (!typed.empty())
                    {
                        layout_profiles::save(typed, system, styling);
                    }
                },
                system,
                30
            ));
        }
        else if (index - 1 < saved.size())
        {
            open_profile_actions(stack, system, styling, saved[index - 1]);
        }
    });
    stack.push(menu);
}