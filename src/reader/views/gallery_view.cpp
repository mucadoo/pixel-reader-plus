#include "./gallery_view.h"

#include "./menu_draw.h"
#include "doc_api/doc_reader.h"
#include "doc_api/doc_token.h"
#include "extern/rotozoom/SDL_rotozoom.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <SDL/SDL.h>

#include <algorithm>

namespace
{

const int SCREEN_W = static_cast<int>(::SCREEN_WIDTH);
const int SCREEN_H = static_cast<int>(::SCREEN_HEIGHT);
constexpr int MAX_DECODE_PIXELS = 12 * 1000 * 1000; // bigger pictures are not decoded for a thumbnail

void fill(SDL_Surface *dest, int x, int y, int w, int h, SDL_Color color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    SDL_Rect rect = {static_cast<Sint16>(x), static_cast<Sint16>(y), static_cast<Uint16>(w), static_cast<Uint16>(h)};
    SDL_FillRect(dest, &rect, SDL_MapRGB(dest->format, color.r, color.g, color.b));
}

} // namespace

std::vector<GalleryPicture> find_pictures(
    const DocReader &reader,
    size_t max_pictures,
    const std::function<void(int percent)> &progress
)
{
    std::vector<GalleryPicture> found;
    auto iter = reader.get_iter(0);
    if (!iter)
    {
        return found;
    }
    int last_percent = -1;
    while (const DocToken *token = iter->read(1))
    {
        if (progress)
        {
            const int percent = static_cast<int>(std::min<uint32_t>(reader.get_global_progress_percent(token->address), 100));
            if (percent != last_percent)
            {
                last_percent = percent;
                progress(percent);
            }
        }
        if (token->type == TokenType::Image)
        {
            found.push_back({token->address, static_cast<const ImageDocToken *>(token)->path});
            if (found.size() >= max_pictures)
            {
                break;
            }
        }
    }
    return found;
}

GalleryView::GalleryView(
    std::shared_ptr<DocReader> reader,
    std::vector<GalleryPicture> pictures,
    std::function<void(const std::filesystem::path &)> on_open,
    std::function<void(DocAddr)> on_go,
    SystemStyling &styling
)
    : reader(std::move(reader))
    , styling(styling)
    , styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          _needs_render = true;
      }))
    , pictures(std::move(pictures))
    , on_open(std::move(on_open))
    , on_go(std::move(on_go))
{
}

GalleryView::~GalleryView()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

int GalleryView::visible_first() const
{
    return first_row * cols;
}

int GalleryView::visible_last() const
{
    return std::min(static_cast<int>(pictures.size()), (first_row + rows) * cols) - 1;
}

void GalleryView::load_thumbnail(int index)
{
    surface_unique_ptr small;
    const std::filesystem::path &path = pictures[index].path;
    std::string extension = path.extension().string();
    if (!extension.empty() && extension[0] == '.')
    {
        extension.erase(0, 1);
    }
    const std::vector<char> data = reader->load_resource(path);
    int w = 0;
    int h = 0;
    if (!data.empty() && image_pixel_size(data.data(), static_cast<uint32_t>(data.size()), w, h)
        && w > 0 && h > 0 && static_cast<long long>(w) * h <= MAX_DECODE_PIXELS)
    {
        surface_unique_ptr full = load_surface_from_ptr(data.data(), static_cast<uint32_t>(data.size()), extension, get_render_surface_format());
        if (full)
        {
            const double scale = std::min(1.0, 160.0 / std::max(full->w, full->h));
            if (scale < 1.0)
            {
                small.reset(zoomSurface(full.get(), scale, scale, 1));
            }
            else
            {
                small = std::move(full);
            }
        }
    }
    thumbnails[index] = std::move(small);
}

bool GalleryView::tick(uint32_t)
{
    for (int index = visible_first(); index <= visible_last(); ++index)
    {
        if (!thumbnails.count(index))
        {
            load_thumbnail(index);
            _needs_render = true;
            return true;
        }
    }
    return false;
}

void GalleryView::move_cursor(int cols_delta, int rows_delta)
{
    const int count = static_cast<int>(pictures.size());
    cursor = std::clamp(cursor + cols_delta + rows_delta * cols, 0, std::max(0, count - 1));
    const int row = cursor / cols;
    if (row < first_row)
    {
        first_row = row;
    }
    else if (row >= first_row + rows)
    {
        first_row = row - rows + 1;
    }
    _needs_render = true;
}

bool GalleryView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!_needs_render && !force_render)
    {
        return false;
    }
    _needs_render = false;

    TTF_Font *font = cached_load_font(styling.get_font_name(), styling.get_font_size());
    if (!font)
    {
        return true;
    }
    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color quiet = menu_draw::quiet_text(theme);
    const int line_height = TTF_FontHeight(font) + 4;

    fill(dest_surface, 0, 0, SCREEN_W, SCREEN_H, theme.background);
    menu_draw::draw_title_bar(
        dest_surface, font, theme, line_height,
        "Pictures",
        std::to_string(std::min<int>(cursor + 1, static_cast<int>(pictures.size()))) + " / " + std::to_string(pictures.size())
    );

    const int top = line_height + 10;
    const int bottom = SCREEN_H - line_height - 10;
    const int gap = 6;
    const int tile_w = (SCREEN_W - gap * (cols + 1)) / cols;
    const int tile_h = (bottom - top - gap * (rows + 1)) / rows;

    for (int index = visible_first(); index <= visible_last(); ++index)
    {
        const int row = index / cols - first_row;
        const int col = index % cols;
        const int x = gap + col * (tile_w + gap);
        const int y = top + gap + row * (tile_h + gap);
        const bool selected = index == cursor;
        fill(dest_surface, x, y, tile_w, tile_h, selected ? theme.highlight_text : quiet);
        fill(dest_surface, x + 2, y + 2, tile_w - 4, tile_h - 4, selected ? theme.highlight_background : theme.background);

        const auto found = thumbnails.find(index);
        if (found != thumbnails.end() && found->second)
        {
            SDL_Surface *picture = found->second.get();
            const double fit = std::min(static_cast<double>(tile_w - 8) / picture->w, static_cast<double>(tile_h - 8) / picture->h);
            surface_unique_ptr shown;
            SDL_Surface *drawn = picture;
            if (fit < 1.0)
            {
                shown.reset(zoomSurface(picture, fit, fit, 1));
                if (shown)
                {
                    drawn = shown.get();
                }
            }
            SDL_Rect at = {
                static_cast<Sint16>(x + (tile_w - drawn->w) / 2),
                static_cast<Sint16>(y + (tile_h - drawn->h) / 2), 0, 0};
            SDL_BlitSurface(drawn, nullptr, dest_surface, &at);
        }
        else if (found != thumbnails.end())
        {
            if (auto surface = menu_draw::shade(font, "?", quiet, selected ? theme.highlight_background : theme.background))
            {
                SDL_Rect at = {
                    static_cast<Sint16>(x + (tile_w - surface->w) / 2),
                    static_cast<Sint16>(y + (tile_h - surface->h) / 2), 0, 0};
                SDL_BlitSurface(surface.get(), nullptr, dest_surface, &at);
            }
        }
    }

    menu_draw::draw_hint_bar(
        dest_surface, font, theme, line_height, {"A  View", "Y  Go to place", "D-pad  Move", "B  Close"}
    );
    return true;
}

bool GalleryView::is_done()
{
    return _is_done;
}

bool GalleryView::is_modal()
{
    return false;
}

void GalleryView::on_keypress(SDLKey key)
{
    switch (key)
    {
        case SW_BTN_LEFT:
            move_cursor(-1, 0);
            break;
        case SW_BTN_RIGHT:
            move_cursor(1, 0);
            break;
        case SW_BTN_UP:
            move_cursor(0, -1);
            break;
        case SW_BTN_DOWN:
            move_cursor(0, 1);
            break;
        case SW_BTN_L1:
        case SW_BTN_L2:
            move_cursor(0, -rows);
            break;
        case SW_BTN_R1:
        case SW_BTN_R2:
            move_cursor(0, rows);
            break;
        case SW_BTN_A:
            if (cursor < static_cast<int>(pictures.size()) && on_open)
            {
                on_open(pictures[cursor].path);
            }
            break;
        case SW_BTN_Y:
            if (cursor < static_cast<int>(pictures.size()))
            {
                _is_done = true;
                mark_completed();
                if (on_go)
                {
                    on_go(pictures[cursor].address);
                }
            }
            return;
        case SW_BTN_B:
        case SW_BTN_MENU:
            _is_done = true;
            return;
        default:
            break;
    }
}