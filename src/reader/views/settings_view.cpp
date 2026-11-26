#include "./settings_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/color_theme_def.h"
#include "reader/font_catalog.h"
#include "reader/dictionary.h"
#include "reader/highlights.h"
#include "reader/screen_dim.h"
#include "reader/layout_options.h"
#include "reader/settings_store.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/picture_filter.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <filesystem>

SettingsView::SettingsView(
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    std::string font_name
) : font_name(font_name),
    sys_styling(sys_styling),
    token_view_styling(token_view_styling),
    styling_sub_id(sys_styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
        needs_render = true;
    })),
    num_menu_items(ROW_COUNT)
{
}

SettingsView::~SettingsView()
{
    sys_styling.unsubscribe_from_changes(styling_sub_id);
}

namespace
{

SDL_Color quiet_text(const ColorTheme &theme)
{
    return SDL_Color {
        static_cast<Uint8>((theme.main_text.r * 2 + theme.background.r) / 3),
        static_cast<Uint8>((theme.main_text.g * 2 + theme.background.g) / 3),
        static_cast<Uint8>((theme.main_text.b * 2 + theme.background.b) / 3),
        0
    };
}

std::string hour_label(int hour)
{
    char text[8];
    std::snprintf(text, sizeof(text), "%02d:00", hour);
    return text;
}

const char *theme_display_name(const std::string &name)
{
    if (name == "spruce") return "Spruce";
    if (name == "night_contrast") return "Night";
    if (name == "light_contrast") return "Light";
    if (name == "light_sepia") return "Sepia";
    if (name == "vampire") return "Vampire";
    return nullptr;
}

std::string shoulder_display_name(const std::string &name)
{
    const std::string &shown = get_shoulder_keymap_display_name(name);
    if (shown == "LR") return "L1 / R1";
    if (shown == "L2R2") return "L2 / R2";
    return shown;
}

surface_unique_ptr shade(TTF_Font *font, const std::string &text, SDL_Color fg, SDL_Color bg)
{
    if (!font || text.empty())
    {
        return nullptr;
    }
    return surface_unique_ptr { TTF_RenderUTF8_Shaded(font, text.c_str(), fg, bg) };
}

void blit_crop(SDL_Surface *src, SDL_Surface *dest, int x, int y, int max_w)
{
    if (!src || max_w <= 0)
    {
        return;
    }
    SDL_Rect src_rect = {0, 0, static_cast<Uint16>(src->w), static_cast<Uint16>(src->h)};
    if (src_rect.w > max_w)
    {
        src_rect.w = static_cast<Uint16>(max_w);
    }
    SDL_Rect dst = {static_cast<Sint16>(x), static_cast<Sint16>(y), 0, 0};
    SDL_BlitSurface(src, &src_rect, dest, &dst);
}

void dim_screen(SDL_Surface *dest)
{
    surface_unique_ptr mask { SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0) };
    if (!mask)
    {
        return;
    }
    SDL_SetAlpha(mask.get(), SDL_SRCALPHA, 140);
    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(mask.get(), &rect, SDL_MapRGB(mask->format, 0, 0, 0));
    SDL_BlitSurface(mask.get(), nullptr, dest, &rect);
}

}

bool SettingsView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!needs_render && !force_render)
    {
        return false;
    }
    needs_render = false;

    TTF_Font *sys_font = cached_load_font(font_name, sys_styling.get_font_size());
    TTF_Font *user_font = sys_styling.get_loaded_font();
    if (!user_font)
    {
        user_font = sys_font;
    }
    const auto &theme = sys_styling.get_loaded_color_theme();
    SDL_Color quiet = quiet_text(theme);

    const char *theme_name = theme_display_name(sys_styling.get_color_theme());
    std::string theme_value = theme_name ? theme_name : sys_styling.get_color_theme();
    std::string night_value = "Off";
    if (!token_view_styling.get_night_theme().empty())
    {
        const char *night_name = theme_display_name(token_view_styling.get_night_theme());
        night_value = night_name ? night_name : token_view_styling.get_night_theme();
    }
    std::string font_value = std::filesystem::path(sys_styling.get_font_name()).filename().stem().string();

    struct Row
    {
        const char *label;
        std::string value;
        bool adjustable;
        TTF_Font *font;
    };
    Row rows[] = {
        {"Theme", theme_value, true, sys_font},
        {"Font size", std::to_string(sys_styling.get_font_size()), true, sys_font},
        {"Font", font_value, true, user_font},
        {"Shoulders", shoulder_display_name(sys_styling.get_shoulder_keymap()), true, sys_font},
        {
            "Progress",
            token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT ? "Chapter" : "Book",
            true,
            sys_font
        },
        {
            "Reading",
            token_view_styling.get_reading_mode() == ReadingMode::Page ? "Page" : "Scroll",
            true,
            sys_font
        },
        {"Bar left", bar_item_label(token_view_styling.get_bar_left()), true, sys_font},
        {"Bar middle", bar_item_label(token_view_styling.get_bar_middle()), true, sys_font},
        {"Bar right", bar_item_label(token_view_styling.get_bar_right()), true, sys_font},
        {"Clock", clock_format_label(token_view_styling.get_clock_format()), true, sys_font},
        {"Screen", rotation_label(token_view_styling.get_rotation()), true, sys_font},
        {"Left margin", margin_label(token_view_styling.get_margin_level()), true, sys_font},
        {"Right margin", margin_label(token_view_styling.get_margin_right()), true, sys_font},
        {"Top margin", vertical_margin_label(token_view_styling.get_margin_top()), true, sys_font},
        {"Bottom margin", vertical_margin_label(token_view_styling.get_margin_bottom()), true, sys_font},
        {"Line spacing", spacing_label(token_view_styling.get_spacing_level()), true, sys_font},
        {"Justify", token_view_styling.get_justify() ? "On" : "Off", true, sys_font},
        {"Indent", indent_label(token_view_styling.get_indent_level()), true, sys_font},
        {"Paragraph gap", token_view_styling.get_paragraph_gap() ? "On" : "Off", true, sys_font},
        {"Bold text", token_view_styling.get_bold_text() ? "On" : "Off", true, sys_font},
        {"Hyphenate", token_view_styling.get_hyphenate() ? "On" : "Off", true, sys_font},
        {"Pictures", picture_mode_label(token_view_styling.get_picture_mode()), true, sys_font},
        {"Thin bar", token_view_styling.get_thin_bar() ? "On" : "Off", true, sys_font},
        {"Word spacing", word_spacing_label(token_view_styling.get_word_spacing()), true, sys_font},
        {"Letter spacing", letter_spacing_label(token_view_styling.get_letter_spacing()), true, sys_font},
        {"Scroll overlap", std::to_string(token_view_styling.get_scroll_overlap()) + (token_view_styling.get_scroll_overlap() == 1 ? " line" : " lines"), true, sys_font},
        {"Reading ruler", ruler_label(token_view_styling.get_ruler()), true, sys_font},
        {"Dictionary", token_view_styling.get_dictionary().empty() ? std::string("Auto") : token_view_styling.get_dictionary(), true, sys_font},
        {"Screen dim", screen_dim::label_for_level(token_view_styling.get_screen_dim()), true, sys_font},
        {"Book styles", token_view_styling.get_book_styles() ? "On" : "Off", true, sys_font},
        {"Highlight color", highlight_color_name(token_view_styling.get_highlight_color()), true, sys_font},
        {"Night theme", night_value, true, sys_font},
        {"Night from", hour_label(token_view_styling.get_night_from()), true, sys_font},
        {"Night until", hour_label(token_view_styling.get_night_until()), true, sys_font},
        {"Auto exit", auto_exit_label(token_view_styling.get_auto_exit_minutes()), true, sys_font},
        {"Cover screen", cover_sleep_label(token_view_styling.get_cover_sleep()), true, sys_font},
        {"Book grid", grid_size_label(token_view_styling.get_grid_size()), true, sys_font},
        {"Style tweaks", style_tweaks_label(token_view_styling.get_tweaks()), false, sys_font},
        {"Layout profiles", "Open", false, sys_font},
        {"Comics", std::string(token_view_styling.get_comic_manga() ? "Manga" : "Western") + (token_view_styling.get_comic_split() ? ", split" : "") + (token_view_styling.get_comic_fit() == 1 ? ", wide" : ""), false, sys_font},
        {"Controls", "Open", false, sys_font},
    };

    dim_screen(dest_surface);
    if (!sys_font)
    {
        return true;
    }

    int font_h = TTF_FontHeight(sys_font);
    if (user_font)
    {
        font_h = std::max(font_h, TTF_FontHeight(user_font));
    }
    const int row_h = font_h + 12;
    const int title_h = font_h + 10;
    const int footer_h = font_h + 8;
    const int pad = 16;
    const int margin = 18;
    const int panel_w = static_cast<int>(SCREEN_WIDTH) - margin * 2;
    const int max_panel_h = static_cast<int>(SCREEN_HEIGHT) - margin * 2;
    int chrome = pad * 2 + title_h + footer_h + 8;
    int shown = std::min(num_menu_items, std::max(1, (max_panel_h - chrome) / std::max(1, row_h)));
    if (shown >= num_menu_items)
    {
        scroll_position = 0;
    }
    else if (line_selected < scroll_position)
    {
        scroll_position = line_selected;
    }
    else if (line_selected >= scroll_position + static_cast<uint32_t>(shown))
    {
        scroll_position = line_selected + 1 - shown;
    }

    int panel_h = chrome + shown * row_h;
    int panel_x = margin;
    int panel_y = (static_cast<int>(SCREEN_HEIGHT) - panel_h) / 2;

    uint32_t bg = SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b);
    uint32_t fg = SDL_MapRGB(dest_surface->format, theme.main_text.r, theme.main_text.g, theme.main_text.b);
    uint32_t hl = SDL_MapRGB(dest_surface->format, theme.highlight_background.r, theme.highlight_background.g, theme.highlight_background.b);
    uint32_t rule = SDL_MapRGB(dest_surface->format, quiet.r, quiet.g, quiet.b);

    SDL_Rect frame = {
        static_cast<Sint16>(panel_x),
        static_cast<Sint16>(panel_y),
        static_cast<Uint16>(panel_w),
        static_cast<Uint16>(panel_h)
    };
    SDL_FillRect(dest_surface, &frame, fg);
    frame.x += 1;
    frame.y += 1;
    frame.w -= 2;
    frame.h -= 2;
    SDL_FillRect(dest_surface, &frame, bg);

    auto heading = shade(sys_font, "Settings", theme.main_text, theme.background);
    blit_crop(
        heading.get(),
        dest_surface,
        panel_x + pad,
        panel_y + pad + std::max(0, (title_h - (heading ? heading->h : 0)) / 2),
        panel_w - pad * 2
    );
    SDL_Rect title_rule = {
        static_cast<Sint16>(panel_x + pad),
        static_cast<Sint16>(panel_y + pad + title_h),
        static_cast<Uint16>(panel_w - pad * 2),
        1
    };
    SDL_FillRect(dest_surface, &title_rule, rule);

    auto left_arrow = shade(sys_font, "<", theme.highlight_text, theme.highlight_background);
    auto right_arrow = shade(sys_font, ">", theme.highlight_text, theme.highlight_background);
    int arrow_w = left_arrow ? left_arrow->w : 12;
    int arrow_slot = arrow_w + 8;

    int y = panel_y + pad + title_h + 8;
    for (int i = 0; i < shown; ++i)
    {
        uint32_t index = scroll_position + static_cast<uint32_t>(i);
        if (index >= static_cast<uint32_t>(num_menu_items))
        {
            break;
        }
        const Row &row = rows[index];
        bool selected = index == line_selected;
        if (selected)
        {
            SDL_Rect bar = {
                static_cast<Sint16>(panel_x + 8),
                static_cast<Sint16>(y),
                static_cast<Uint16>(panel_w - 16),
                static_cast<Uint16>(row_h)
            };
            SDL_FillRect(dest_surface, &bar, hl);
        }

        SDL_Color label_fg = selected ? theme.highlight_text : quiet;
        SDL_Color label_bg = selected ? theme.highlight_background : theme.background;
        SDL_Color value_fg = selected ? theme.highlight_text : theme.main_text;
        SDL_Color value_bg = selected ? theme.highlight_background : theme.background;

        auto label = shade(sys_font, row.label, label_fg, label_bg);
        auto value = shade(row.font ? row.font : sys_font, row.value, value_fg, value_bg);
        int label_w = label ? label->w : 0;
        int value_w = value ? value->w : 0;
        int label_x = panel_x + pad;
        int value_right = panel_x + panel_w - pad - arrow_slot;
        int label_limit = value_right - label_x - 16 - (selected && row.adjustable ? arrow_slot : 0);
        if (label_w > label_limit)
        {
            label_w = std::max(24, label_limit);
        }
        int value_limit = value_right - (label_x + label_w + 16);
        if (value_limit < 24)
        {
            value_limit = 24;
        }
        if (value_w > value_limit)
        {
            value_w = value_limit;
        }
        int value_x = value_right - value_w;
        int text_y = y + std::max(0, (row_h - font_h) / 2);

        blit_crop(label.get(), dest_surface, label_x, text_y, value_x - label_x - 12);
        blit_crop(value.get(), dest_surface, value_x, text_y, value_w);

        if (selected && row.adjustable)
        {
            int arrow_y = y + std::max(0, (row_h - (left_arrow ? left_arrow->h : 0)) / 2);
            blit_crop(left_arrow.get(), dest_surface, value_x - arrow_slot, arrow_y, arrow_w);
            blit_crop(right_arrow.get(), dest_surface, value_right + 8, arrow_y, arrow_w);
        }

        y += row_h;
    }

    const char *footer_left = row_actions.count(line_selected) ? "A  Open" : "Left / right";
    auto footer = shade(sys_font, footer_left, quiet, theme.background);
    auto close = shade(sys_font, "B  Close", quiet, theme.background);
    int footer_y = panel_y + panel_h - pad - footer_h;
    SDL_Rect footer_rule = {
        static_cast<Sint16>(panel_x + pad),
        static_cast<Sint16>(footer_y),
        static_cast<Uint16>(panel_w - pad * 2),
        1
    };
    SDL_FillRect(dest_surface, &footer_rule, rule);
    int hint_y = footer_y + std::max(0, (footer_h - (footer ? footer->h : 0)) / 2);
    blit_crop(footer.get(), dest_surface, panel_x + pad, hint_y, panel_w / 2);
    if (close)
    {
        blit_crop(close.get(), dest_surface, panel_x + panel_w - pad - close->w, hint_y, close->w);
    }

    return true;
}

bool SettingsView::is_done()
{
    return _is_done;
}

bool SettingsView::is_modal()
{
    return true;
}

void SettingsView::on_change_theme(int dir)
{
    std::string theme = sys_styling.get_color_theme();
    sys_styling.set_color_theme(
        (dir < 0) ?
            get_prev_theme(theme) :
            get_next_theme(theme)
    );
}

void SettingsView::on_change_font_size(int dir)
{
    sys_styling.set_font_size(
        (dir < 0) ?
            sys_styling.get_prev_font_size() :
            sys_styling.get_next_font_size()
    );
}

void SettingsView::on_change_font_name(int dir)
{
    std::string font_name = sys_styling.get_font_name();
    sys_styling.set_font_name(
        (dir < 0) ?
            get_prev_font_name(font_name) :
            get_next_font_name(font_name)
    );
}

void SettingsView::on_change_shoulder_keymap(int dir)
{
    const auto &keymap = sys_styling.get_shoulder_keymap();
    sys_styling.set_shoulder_keymap(
        (dir < 0) ?
            get_prev_shoulder_keymap(keymap) :
            get_next_shoulder_keymap(keymap)
    );
}

void SettingsView::on_change_progress()
{
    token_view_styling.set_progress_reporting(
        get_next_progress_reporting(token_view_styling.get_progress_reporting())
    );
}

void SettingsView::on_change_bar(int slot, int dir)
{
    auto step = [dir](BarItem item) {
        return dir < 0 ? get_prev_bar_item(item) : get_next_bar_item(item);
    };
    if (slot == 0)
    {
        token_view_styling.set_bar_left(step(token_view_styling.get_bar_left()));
    }
    else if (slot == 1)
    {
        token_view_styling.set_bar_middle(step(token_view_styling.get_bar_middle()));
    }
    else
    {
        token_view_styling.set_bar_right(step(token_view_styling.get_bar_right()));
    }
}

void SettingsView::on_change_reading()
{
    token_view_styling.set_reading_mode(
        get_next_reading_mode(token_view_styling.get_reading_mode())
    );
}

void SettingsView::on_keypress(SDLKey key)
{
    switch (key) {
        case SW_BTN_UP:
            line_selected = (line_selected + num_menu_items - 1) % num_menu_items;
            needs_render = true;
            break;
        case SW_BTN_DOWN:
            line_selected = (line_selected + 1) % num_menu_items;
            needs_render = true;
            break;
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            {
                int dir = (key == SW_BTN_LEFT) ? -1 : 1;
                if (line_selected == 0)
                {
                    on_change_theme(dir);
                }
                else if (line_selected == 1)
                {
                    on_change_font_size(dir);
                }
                else if (line_selected == 2)
                {
                    on_change_font_name(dir);
                }
                else if (line_selected == 3)
                {
                    on_change_shoulder_keymap(dir);
                }
                else if (line_selected == 4)
                {
                    on_change_progress();
                }
                else if (line_selected == 5)
                {
                    on_change_reading();
                }
                else if (line_selected >= ROW_BAR_LEFT && line_selected <= ROW_BAR_RIGHT)
                {
                    on_change_bar(static_cast<int>(line_selected - ROW_BAR_LEFT), dir);
                }
                else if (line_selected == ROW_CLOCK)
                {
                    token_view_styling.set_clock_format(
                        get_next_clock_format(token_view_styling.get_clock_format())
                    );
                }
                else if (line_selected == ROW_MARGINS)
                {
                    token_view_styling.set_margin_level(
                        (token_view_styling.get_margin_level() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_MARGIN_RIGHT)
                {
                    token_view_styling.set_margin_right(
                        (token_view_styling.get_margin_right() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_MARGIN_TOP)
                {
                    token_view_styling.set_margin_top(
                        (token_view_styling.get_margin_top() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_MARGIN_BOTTOM)
                {
                    token_view_styling.set_margin_bottom(
                        (token_view_styling.get_margin_bottom() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_RULER)
                {
                    token_view_styling.set_ruler((token_view_styling.get_ruler() + RULER_MODE_COUNT + dir) % RULER_MODE_COUNT);
                }
                else if (line_selected == ROW_SPACING)
                {
                    token_view_styling.set_spacing_level(
                        (token_view_styling.get_spacing_level() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_JUSTIFY)
                {
                    token_view_styling.set_justify(!token_view_styling.get_justify());
                }
                else if (line_selected == ROW_INDENT)
                {
                    token_view_styling.set_indent_level(
                        (token_view_styling.get_indent_level() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_GAP)
                {
                    token_view_styling.set_paragraph_gap(!token_view_styling.get_paragraph_gap());
                }
                else if (line_selected == ROW_BOLD)
                {
                    token_view_styling.set_bold_text(!token_view_styling.get_bold_text());
                }
                else if (line_selected == ROW_HYPHEN)
                {
                    token_view_styling.set_hyphenate(!token_view_styling.get_hyphenate());
                }
                else if (line_selected == ROW_PICTURES)
                {
                    token_view_styling.set_picture_mode(
                        (token_view_styling.get_picture_mode() + PICTURE_MODE_COUNT + dir) % PICTURE_MODE_COUNT
                    );
                }
                else if (line_selected == ROW_THIN_BAR)
                {
                    token_view_styling.set_thin_bar(!token_view_styling.get_thin_bar());
                }
                else if (line_selected == ROW_WORD_SPACING)
                {
                    token_view_styling.set_word_spacing(
                        (token_view_styling.get_word_spacing() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_ROTATION)
                {
                    token_view_styling.set_rotation((token_view_styling.get_rotation() + 3 + dir) % 3);
                }
                else if (line_selected == ROW_LETTER_SPACING)
                {
                    token_view_styling.set_letter_spacing(
                        (token_view_styling.get_letter_spacing() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_OVERLAP)
                {
                    token_view_styling.set_scroll_overlap(
                        (token_view_styling.get_scroll_overlap() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_GRID)
                {
                    token_view_styling.set_grid_size((token_view_styling.get_grid_size() + 3 + dir) % 3);
                }
                else if (line_selected == ROW_COVER_SLEEP)
                {
                    token_view_styling.set_cover_sleep(
                        (token_view_styling.get_cover_sleep() + LAYOUT_LEVEL_COUNT + dir) % LAYOUT_LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_DICTIONARY)
                {
                    std::vector<std::string> choices = {""};
                    for (const std::string &name : dictionary_names())
                    {
                        choices.push_back(name);
                    }
                    size_t at = 0;
                    for (size_t i = 0; i < choices.size(); ++i)
                    {
                        if (choices[i] == token_view_styling.get_dictionary())
                        {
                            at = i;
                        }
                    }
                    at = (at + choices.size() + static_cast<size_t>(dir + static_cast<int>(choices.size()))) % choices.size();
                    token_view_styling.set_dictionary(choices[at]);
                }
                else if (line_selected == ROW_SCREEN_DIM)
                {
                    token_view_styling.set_screen_dim(
                        (token_view_styling.get_screen_dim() + screen_dim::LEVEL_COUNT + dir) % screen_dim::LEVEL_COUNT
                    );
                }
                else if (line_selected == ROW_BOOK_STYLES)
                {
                    token_view_styling.set_book_styles(!token_view_styling.get_book_styles());
                }
                else if (line_selected == ROW_HILITE)
                {
                    token_view_styling.set_highlight_color(
                        (token_view_styling.get_highlight_color() + HIGHLIGHT_COLOR_COUNT + dir) % HIGHLIGHT_COLOR_COUNT
                    );
                }
                else if (line_selected == ROW_NIGHT)
                {
                    // Off, then each theme in turn
                    int index = -1;
                    for (int i = 0; i < theme_count(); ++i)
                    {
                        if (theme_name_at(i) == token_view_styling.get_night_theme())
                        {
                            index = i;
                        }
                    }
                    const int span = theme_count() + 1;
                    index = (index + 1 + dir + span) % span - 1;
                    token_view_styling.set_night_theme(index < 0 ? std::string() : theme_name_at(index));
                }
                else if (line_selected == ROW_NIGHT_FROM)
                {
                    token_view_styling.set_night_from(token_view_styling.get_night_from() + dir);
                }
                else if (line_selected == ROW_NIGHT_UNTIL)
                {
                    token_view_styling.set_night_until(token_view_styling.get_night_until() + dir);
                }
                else if (line_selected == ROW_AUTO_EXIT)
                {
                    const int step = auto_exit_step_for_minutes(token_view_styling.get_auto_exit_minutes());
                    token_view_styling.set_auto_exit_minutes(
                        auto_exit_minutes_for_step((step + AUTO_EXIT_STEP_COUNT + dir) % AUTO_EXIT_STEP_COUNT)
                    );
                }
                needs_render = true;
            }
            break;
        case SW_BTN_A:
            if (const auto action = row_actions.find(line_selected); action != row_actions.end() && action->second)
            {
                action->second();
            }
            break;
        case SW_BTN_B:
            {
                _is_done = true;
            }
            break;
        default:
            break;
    }
}

void SettingsView::set_controls_action(std::function<void()> action)
{
    row_actions[ROW_CONTROLS] = std::move(action);
}

void SettingsView::set_row_action(uint32_t row, std::function<void()> action)
{
    row_actions[row] = std::move(action);
}

void SettingsView::terminate()
{
    _is_done = true;
}

void SettingsView::unterminate()
{
    _is_done = false;
}
