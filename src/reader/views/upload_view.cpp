#include "./upload_view.h"

#include "./menu_draw.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_utils.h"

#include <algorithm>

using namespace menu_draw;

UploadView::UploadView(std::filesystem::path folder, SystemStyling &styling, std::function<void()> on_finished)
    : server(folder)
    , folder(std::move(folder))
    , on_finished(std::move(on_finished))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
{
    const std::string ip = local_ip_address();
    if (ip.empty())
    {
        problem = "No wifi connection found. Turn wifi on, join your network and try again.";
    }
    else
    {
        std::string error;
        if (server.start(8080, error))
        {
            address = "http://" + ip + ":" + std::to_string(server.get_port());
        }
        else
        {
            problem = error;
        }
    }
}

UploadView::~UploadView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

bool UploadView::render(SDL_Surface *dest, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *font = styling.get_loaded_font();
    const ColorTheme &theme = styling.get_loaded_color_theme();
    const int line_h = detect_line_height(font) + 4;
    const int width = static_cast<int>(SCREEN_WIDTH);
    const int height = static_cast<int>(SCREEN_HEIGHT);
    const int pad = 14;

    SDL_Rect all = {0, 0, static_cast<Uint16>(width), static_cast<Uint16>(height)};
    SDL_FillRect(dest, &all, SDL_MapRGB(dest->format, theme.background.r, theme.background.g, theme.background.b));
    draw_title_bar(dest, font, theme, line_h, "Receive books over wifi", "");

    int y = line_h * 2;
    auto text = [&](const std::string &value, bool quiet) {
        for (const std::string &piece : wrap_lines(font, value, width - pad * 2, 6))
        {
            auto surface = shade(font, piece, quiet ? quiet_text(theme) : theme.main_text, theme.background);
            if (surface)
            {
                blit_crop(surface.get(), dest, pad, y, width - pad * 2);
            }
            y += line_h;
        }
    };

    if (!problem.empty())
    {
        text(problem, false);
    }
    else
    {
        text("On a phone or computer on the same wifi, open this address in a browser:", true);
        y += line_h / 2;
        text(address, false);
        y += line_h / 2;
        text("Choose the books and press Send. They are saved in the Downloads folder of the library. Anyone on your wifi can send books while this screen is open.", true);
        y += line_h / 2;
        const auto &files = server.files();
        if (!server.message().empty())
        {
            text(server.message(), false);
        }
        const size_t shown = std::min<size_t>(files.size(), static_cast<size_t>(std::max(0, (height - y - line_h * 2) / line_h)));
        for (size_t i = files.size() - shown; i < files.size(); ++i)
        {
            text("+ " + files[i], true);
        }
    }
    draw_hint_bar(dest, font, theme, line_h, {"B  Stop"});
    return true;
}

bool UploadView::is_done()
{
    return _is_done;
}

bool UploadView::is_modal()
{
    return false;
}

void UploadView::on_keypress(SDLKey key)
{
    if (key == SW_BTN_B || key == SW_BTN_SELECT)
    {
        _is_done = true;
    }
}

bool UploadView::tick(uint32_t)
{
    if (server.poll())
    {
        _needs_render = true;
        return true;
    }
    return false;
}

void UploadView::on_pop()
{
    server.stop();
    if (on_finished)
    {
        on_finished();
    }
}