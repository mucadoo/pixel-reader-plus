#include "./reading_history_view.h"

#include "./menu_draw.h"
#include "reader/reading_stats.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace menu_draw;

namespace
{

constexpr const char *MONTHS[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
constexpr const char *WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

int days_in_month(int year, int month)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    {
        return 29;
    }
    return lengths[month];
}

// A moment at noon on a day, so a change of clock never moves it to another date.
long long noon_of(int year, int month, int day)
{
    std::tm when {};
    when.tm_year = year - 1900;
    when.tm_mon = month;
    when.tm_mday = day;
    when.tm_hour = 12;
    when.tm_isdst = -1;
    return static_cast<long long>(std::mktime(&when));
}

std::string key_of(int year, int month, int day)
{
    return reading_stats::day_for(noon_of(year, month, day));
}

// 0 for Sunday.
int weekday_of(int year, int month, int day)
{
    const std::time_t t = static_cast<std::time_t>(noon_of(year, month, day));
    std::tm local {};
    localtime_r(&t, &local);
    return local.tm_wday;
}

} // namespace

ReadingHistoryView::ReadingHistoryView(SystemStyling &styling, std::function<std::string(const std::string &)> title_for)
    : styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , title_for(std::move(title_for))
{
    const std::time_t now = std::time(nullptr);
    std::tm local {};
    localtime_r(&now, &local);
    year = local.tm_year + 1900;
    month = local.tm_mon;
    day = local.tm_mday;
}

ReadingHistoryView::~ReadingHistoryView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void ReadingHistoryView::shift_days(int delta)
{
    const long long moved = noon_of(year, month, day + delta);
    const std::time_t t = static_cast<std::time_t>(moved);
    std::tm local {};
    localtime_r(&t, &local);
    year = local.tm_year + 1900;
    month = local.tm_mon;
    day = local.tm_mday;
    _needs_render = true;
}

void ReadingHistoryView::shift_months(int delta)
{
    int total = year * 12 + month + delta;
    year = total / 12;
    month = total % 12;
    day = std::min(day, days_in_month(year, month));
    _needs_render = true;
}

bool ReadingHistoryView::render(SDL_Surface *dest, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *font = styling.get_loaded_font();
    const ColorTheme &theme = styling.get_loaded_color_theme();
    line_h = detect_line_height(font) + 4;
    const int width = static_cast<int>(SCREEN_WIDTH);
    const int height = static_cast<int>(SCREEN_HEIGHT);
    const auto color = [&](SDL_Color c) { return SDL_MapRGB(dest->format, c.r, c.g, c.b); };
    const SDL_Color quiet = quiet_text(theme);

    SDL_Rect all = {0, 0, static_cast<Uint16>(width), static_cast<Uint16>(height)};
    SDL_FillRect(dest, &all, color(theme.background));

    const int total_days = days_in_month(year, month);
    std::vector<long long> seconds(static_cast<size_t>(total_days) + 1, 0);
    long long month_total = 0;
    long long best = 0;
    int days_read = 0;
    for (int d = 1; d <= total_days; ++d)
    {
        seconds[static_cast<size_t>(d)] = reading_stats::seconds_on_day(key_of(year, month, d));
        month_total += seconds[static_cast<size_t>(d)];
        best = std::max(best, seconds[static_cast<size_t>(d)]);
        if (seconds[static_cast<size_t>(d)] >= 60)
        {
            ++days_read;
        }
    }

    const int top = line_h + 6;
    const int detail_h = line_h * 4 + 8;
    const int bottom = height - line_h; // the hint bar
    const int pad = 10;
    const int cell_w = (width - pad * 2) / 7;
    const int head_h = line_h;
    const int rows = (weekday_of(year, month, 1) + total_days + 6) / 7;
    const int cell_h = std::max(line_h + 8, std::min(cell_w, (bottom - top - detail_h - head_h) / std::max(1, rows)));
    const int grid_x = (width - cell_w * 7) / 2;

    for (int i = 0; i < 7; ++i)
    {
        auto label = shade(font, WEEKDAYS[i], quiet, theme.background);
        if (label)
        {
            blit_crop(label.get(), dest, grid_x + i * cell_w + 3, top + 2, cell_w - 3);
        }
    }

    const std::string today = reading_stats::day_for(static_cast<long long>(std::time(nullptr)));
    for (int d = 1; d <= total_days; ++d)
    {
        const int slot = weekday_of(year, month, 1) + d - 1;
        const int cx = grid_x + (slot % 7) * cell_w;
        const int cy = top + head_h + (slot / 7) * cell_h;
        const bool selected = d == day;
        SDL_Rect cell = {static_cast<Sint16>(cx + 1), static_cast<Sint16>(cy + 1), static_cast<Uint16>(cell_w - 2), static_cast<Uint16>(cell_h - 2)};
        if (selected)
        {
            SDL_FillRect(dest, &cell, color(theme.highlight_background));
        }
        else if (key_of(year, month, d) == today)
        {
            SDL_Rect outline = cell;
            SDL_FillRect(dest, &outline, color(quiet));
            outline.x += 1;
            outline.y += 1;
            outline.w -= 2;
            outline.h -= 2;
            SDL_FillRect(dest, &outline, color(theme.background));
        }
        const SDL_Color number_fg = selected ? theme.highlight_text : theme.main_text;
        const SDL_Color number_bg = selected ? theme.highlight_background : theme.background;
        auto number = shade(font, std::to_string(d), number_fg, number_bg);
        if (number)
        {
            blit_crop(number.get(), dest, cx + 4, cy + 2, cell_w - 6);
        }
        // The bar: as tall as the day's time against the best day of the month
        const long long value = seconds[static_cast<size_t>(d)];
        if (value > 0 && best > 0)
        {
            const int room = cell_h - line_h - 8;
            const int bar_h = std::max(2, static_cast<int>(room * value / best));
            SDL_Rect bar = {
                static_cast<Sint16>(cx + 5),
                static_cast<Sint16>(cy + cell_h - 4 - bar_h),
                static_cast<Uint16>(std::max(2, cell_w - 10)),
                static_cast<Uint16>(bar_h)
            };
            SDL_FillRect(dest, &bar, color(selected ? theme.highlight_text : number_fg));
        }
    }

    // What the chosen day holds
    const int detail_y = bottom - detail_h;
    SDL_Rect rule = {static_cast<Sint16>(pad), static_cast<Sint16>(detail_y), static_cast<Uint16>(width - pad * 2), 1};
    SDL_FillRect(dest, &rule, color(quiet));
    const std::string key = key_of(year, month, day);
    const long long chosen = seconds[static_cast<size_t>(day)];
    char heading[64];
    std::snprintf(heading, sizeof(heading), "%s %d: %s", MONTHS[month], day, chosen > 0 ? reading_stats::format_duration(chosen).c_str() : "no reading");
    int y = detail_y + 4;
    if (auto line = shade(font, heading, theme.main_text, theme.background))
    {
        blit_crop(line.get(), dest, pad, y, width - pad * 2);
    }
    y += line_h;
    const auto books = reading_stats::books_on_day(key);
    for (size_t i = 0; i < books.size() && i < 3; ++i)
    {
        std::string name = title_for ? title_for(books[i].first.string()) : books[i].first.stem().string();
        const std::string time = reading_stats::format_duration(books[i].second);
        auto time_surface = shade(font, time, quiet, theme.background);
        const int time_w = time_surface ? time_surface->w : 0;
        if (auto line = shade(font, ellipsize(font, name, width - pad * 2 - time_w - 12), quiet, theme.background))
        {
            blit_crop(line.get(), dest, pad, y, width - pad * 2 - time_w - 8);
        }
        if (time_surface)
        {
            blit_crop(time_surface.get(), dest, width - pad - time_w, y, time_w);
        }
        y += line_h;
    }

    char counter[64];
    std::snprintf(counter, sizeof(counter), "%d days  %s", days_read, reading_stats::format_duration(month_total).c_str());
    draw_title_bar(dest, font, theme, line_h, std::string(MONTHS[month]) + " " + std::to_string(year), counter);
    draw_hint_bar(dest, font, theme, line_h, {"Arrows  Day/Week", "L1/R1  Month", "B  Close"});
    return true;
}

bool ReadingHistoryView::is_done()
{
    return _is_done;
}

bool ReadingHistoryView::is_modal()
{
    return false;
}

void ReadingHistoryView::on_keypress(SDLKey key)
{
    if (key == SW_BTN_LEFT) shift_days(-1);
    else if (key == SW_BTN_RIGHT) shift_days(1);
    else if (key == SW_BTN_UP) shift_days(-7);
    else if (key == SW_BTN_DOWN) shift_days(7);
    else if (key == SW_BTN_L1) shift_months(-1);
    else if (key == SW_BTN_R1) shift_months(1);
    else if (key == SW_BTN_B || key == SW_BTN_A || key == SW_BTN_SELECT) _is_done = true;
}

void ReadingHistoryView::on_keyheld(SDLKey key, uint32_t)
{
    if (key == SW_BTN_LEFT || key == SW_BTN_RIGHT || key == SW_BTN_UP || key == SW_BTN_DOWN)
    {
        on_keypress(key);
    }
}