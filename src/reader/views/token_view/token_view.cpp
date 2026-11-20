#include "./token_view.h"

#include "./token_line_scroller.h"
#include "./token_view_styling.h"

#include "doc_api/doc_reader.h"
#include "reader/highlights.h"
#include "reader/system_styling.h"
#include "reader/shoulder_keymap.h"
#include "reader/text_style.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"
#include "util/throttled.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
namespace {

bool line_fits_on_screen(TTF_Font *font, int avail_width, const char *s, uint32_t len)
{
    int w = avail_width, h;

    char *mut_s = (char*)s;
    char replaced = mut_s[len];
    mut_s[len] = 0;

    TTF_SizeUTF8(font, mut_s, &w, &h);
    mut_s[len] = replaced;

    return w <= avail_width;
}

}  // namespace

struct TokenViewState
{
    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    const uint32_t sys_styling_sub_id;
    const uint32_t token_view_styling_sub_id;

    TTF_Font *current_font = nullptr;

    const int line_padding = 4;
    int line_height;

    TokenLineScroller line_scroller;

    bool needs_render = true;

    std::string title;
    int title_progress_percent = 0;

    std::function<void(DocAddr)> on_scroll;

    Throttled line_scroll_throttle;
    Throttled page_scroll_throttle;
    Throttled word_move_throttle;

    std::filesystem::path highlights_path;
    std::vector<Highlight> highlights;
    bool selecting = false;
    bool has_selection = false;
    DocAddr selected_begin = 0;
    std::vector<LayoutWord> page_words;

    struct VisibleText
    {
        bool present = false;
        int origin = 0;
        int text_y = 0;
        int text_h = 0;
        LineLayout layout;
    };
    std::vector<VisibleText> visible;

    TTF_Font *font_for(float scale, bool pre, int style) const
    {
        uint32_t base = sys_styling.get_font_size();
        if (scale < 0.2f)
        {
            scale = 1.f;
        }
        uint32_t size = static_cast<uint32_t>(static_cast<float>(base) * scale + 0.5f);
        if (size < 12) size = 12;
        if (size > 72) size = 72;

        std::string path = pre
            ? "resources/fonts/DejaVuSansMono.ttf"
            : sys_styling.get_font_name();

        auto try_bold_file = [&](const std::string &candidate) -> TTF_Font * {
            if (!(style & TTF_STYLE_BOLD))
            {
                return nullptr;
            }
            std::string bold_path = candidate;
            auto replace = [&](const char *from, const char *to) {
                auto pos = bold_path.rfind(from);
                if (pos == std::string::npos)
                {
                    return false;
                }
                bold_path.replace(pos, std::strlen(from), to);
                return true;
            };
            if (!replace("-Regular", "-Bold") && !replace("-Light", "-Bold"))
            {
                return nullptr;
            }
            if (!std::filesystem::exists(bold_path))
            {
                return nullptr;
            }
            int rest = style & ~TTF_STYLE_BOLD;
            return cached_load_styled_font(bold_path, size, rest == 0 ? TTF_STYLE_NORMAL : rest);
        };

        if (TTF_Font *bold_font = try_bold_file(path))
        {
            return bold_font;
        }
        TTF_Font *font = cached_load_styled_font(path, size, style);
        if (!font && pre)
        {
            font = cached_load_styled_font(sys_styling.get_font_name(), size, style);
        }
        return font;
    }

    void remember_highlights() const
    {
        if (!highlights_path.empty())
        {
            save_highlights(highlights_path, highlights);
        }
    }

    int num_display_lines() const
    {
        return SCREEN_HEIGHT / line_height;
    }

    int num_text_display_lines() const
    {
        bool show_title_bar = token_view_styling.get_show_title_bar();
        return num_display_lines() - (show_title_bar ? 1 : 0);
    }

    int excess_pxl_y() const
    {
        return SCREEN_HEIGHT - num_display_lines() * line_height;
    }

    int line_pxl_limit_y() const
    {
        if (!token_view_styling.get_show_title_bar())
        {
            return SCREEN_HEIGHT;
        }

        return SCREEN_HEIGHT - line_height - excess_pxl_y() / 2;
    }

    TokenViewState(std::shared_ptr<DocReader> reader, DocAddr address, SystemStyling &sys_styling, TokenViewStyling &token_view_styling)
        : sys_styling(sys_styling),
          token_view_styling(token_view_styling),
          sys_styling_sub_id(sys_styling.subscribe_to_changes([this](SystemStyling::ChangeId change_id) {
              if (change_id == SystemStyling::ChangeId::FONT_SIZE || change_id == SystemStyling::ChangeId::FONT_NAME)
              {
                  current_font = this->sys_styling.get_loaded_font();
                  line_height = detect_line_height(current_font) + line_padding;
                  line_scroller.set_line_height_pixels(line_height);
                  line_scroller.reset_buffer();  // need to re-wrap lines if font-size changed
              }
              needs_render = true;
          })),
          token_view_styling_sub_id(token_view_styling.subscribe_to_changes([this]() {
              needs_render = true;
          })),
          current_font(sys_styling.get_loaded_font()),
          line_height(detect_line_height(sys_styling.get_font_name(), sys_styling.get_font_size()) + line_padding),
          line_scroller(
              reader,
              address,
              [this](const char *s, uint32_t len) {
                  return line_fits_on_screen(
                      current_font,
                      SCREEN_WIDTH - line_padding * 2,
                      s,
                      len
                  );
              },
              line_height,
              [this](float scale, bool pre, int style) {
                  return this->font_for(scale, pre, style);
              }
          ),
          line_scroll_throttle(250, 50),
          page_scroll_throttle(750, 150),
          word_move_throttle(180, 40)
    {
    }

    ~TokenViewState()
    {
        sys_styling.unsubscribe_from_changes(sys_styling_sub_id);
        token_view_styling.unsubscribe_from_changes(token_view_styling_sub_id);
    }
};

TokenView::TokenView(
    std::shared_ptr<DocReader> reader,
    DocAddr address,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    std::filesystem::path highlights_path
) : state(std::make_unique<TokenViewState>(reader, address, sys_styling, token_view_styling))
{
    state->highlights_path = std::move(highlights_path);
    if (!state->highlights_path.empty())
    {
        state->highlights = load_highlights(state->highlights_path);
    }
}

TokenView::~TokenView()
{
}

static void layout_visible_words(TokenViewState &state)
{
    state.page_words.clear();
    int n = state.num_text_display_lines();
    state.visible.assign(n, TokenViewState::VisibleText());
    const int line_height = state.line_height;
    const int line_padding = state.line_padding;
    int line_y = state.excess_pxl_y() / 2;

    for (int i = 0; i < n; ++i)
    {
        const DisplayLine *line = state.line_scroller.get_line_relative(i);
        if (!line)
        {
            break;
        }
        if (line->type == DisplayLine::Type::Text)
        {
            const auto *text_line = static_cast<const TextLine *>(line);
            if (!text_line->continuation && !text_line->text.empty())
            {
                bool base_bold = text_line->bold || text_line->header_level > 0;
                bool base_italic = text_line->italic || text_line->quote;
                auto font_for = [&](int style, float scale) {
                    return state.font_for(scale, text_line->pre, style);
                };
                LineLayout layout = layout_line(
                    text_line->text,
                    text_line->spans,
                    base_bold,
                    base_italic,
                    text_line->font_scale,
                    font_for,
                    text_line->address,
                    text_line->address_lead
                );
                int indent = text_line->quote ? 18 : 0;
                int avail = SCREEN_WIDTH - line_padding * 2 - indent;
                int origin = line_padding + indent;
                if (text_line->centered)
                {
                    origin += std::max(0, (avail - layout.width) / 2);
                }
                int box_h = std::max(1, text_line->slot_span) * line_height;
                int text_h = line_height - line_padding;
                for (const DrawnPiece &piece : layout.pieces)
                {
                    text_h = std::max(text_h, piece.h);
                }
                int text_y = line_y + std::max(0, (box_h - text_h) / 2);

                TokenViewState::VisibleText visual;
                visual.present = true;
                visual.origin = origin;
                visual.text_y = text_y;
                visual.text_h = text_h;
                visual.layout = layout;
                for (LayoutWord &word : visual.layout.words)
                {
                    word.x += origin;
                    word.line_index = i;
                    state.page_words.push_back(word);
                }
                state.visible[i] = std::move(visual);
            }
        }
        line_y += line_height;
    }
}

static void draw_text_line(
    TokenViewState &state,
    SDL_Surface *dest_surface,
    const TextLine *text_line,
    int line_index,
    int,
    const ColorTheme &theme
)
{
    if (line_index < 0 || line_index >= static_cast<int>(state.visible.size()))
    {
        return;
    }
    const TokenViewState::VisibleText &visual = state.visible[line_index];
    if (!visual.present)
    {
        return;
    }

    int lum = theme.background.r + theme.background.g + theme.background.b;
    SDL_Color marker = lum < 380
        ? SDL_Color{120, 92, 40, 0}
        : SDL_Color{255, 224, 120, 0};

    for (const LayoutWord &word : visual.layout.words)
    {
        bool selected = state.selecting && state.has_selection && word.begin == state.selected_begin;
        bool marked = highlights_cover(state.highlights, word.begin);
        if (!selected && !marked)
        {
            continue;
        }
        SDL_Color color = selected ? theme.highlight_background : marker;
        SDL_Rect rect = {
            static_cast<Sint16>(word.x),
            static_cast<Sint16>(visual.text_y),
            static_cast<Uint16>(std::max(1, word.w)),
            static_cast<Uint16>(std::max(1, visual.text_h))
        };
        SDL_FillRect(
            dest_surface,
            &rect,
            SDL_MapRGB(dest_surface->format, color.r, color.g, color.b)
        );
    }

    auto font_for = [&](int style, float scale) {
        return state.font_for(scale, text_line->pre, style);
    };
    int max_ascent = 0;
    for (const DrawnPiece &piece : visual.layout.pieces)
    {
        max_ascent = std::max(max_ascent, piece.ascent);
    }
    for (const DrawnPiece &piece : visual.layout.pieces)
    {
        if (piece.text.empty() || piece.w <= 0)
        {
            continue;
        }
        TTF_Font *run_font = font_for(piece.style, piece.scale);
        if (!run_font)
        {
            continue;
        }
        SDL_Surface *surface = TTF_RenderUTF8_Blended(run_font, piece.text.c_str(), theme.main_text);
        if (!surface)
        {
            continue;
        }
        int dy = max_ascent - piece.ascent;
        SDL_Rect dest = {
            static_cast<Sint16>(piece.x + visual.origin),
            static_cast<Sint16>(visual.text_y + dy),
            0,
            0
        };
        SDL_BlitSurface(surface, nullptr, dest_surface, &dest);
        SDL_FreeSurface(surface);
    }
}

bool TokenView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!state->needs_render && !force_render)
    {
        return false;
    }
    state->needs_render = false;

    scroll(0);  // Will adjust scroll position if necessary for end of book
    layout_visible_words(*state);

    TTF_Font *font = state->current_font;
    const auto &theme = state->sys_styling.get_loaded_color_theme();
    const int line_height = state->line_height;
    const int line_padding = state->line_padding;

    // Clear screen
    {
        SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        const auto &bgcolor = theme.background;

        SDL_FillRect(
            dest_surface,
            &rect,
            SDL_MapRGB(dest_surface->format, bgcolor.r, bgcolor.g, bgcolor.b)
        );
    }

    int num_text_display_lines = state->num_text_display_lines();
    const Uint16 padding_y = state->excess_pxl_y() / 2;
    Sint16 line_y = padding_y;

    for (int i = 0; i < num_text_display_lines; ++i)
    {
        const DisplayLine *line = state->line_scroller.get_line_relative(i);
        if (line)
        {
            if (line->type == DisplayLine::Type::Text)
            {
                draw_text_line(*state, dest_surface, static_cast<const TextLine *>(line), i, line_y, theme);
            }
            else if (line->type == DisplayLine::Type::Image || (line->type == DisplayLine::Type::ImageRef && i == 0))
            {
                const ImageLine *image_line = nullptr;
                uint32_t line_offset = 0;

                if (line->type == DisplayLine::Type::ImageRef)
                {
                    line_offset = static_cast<const ImageRefLine *>(line)->offset;
                    const DisplayLine *ref_line = state->line_scroller.get_line_relative(i - line_offset);
                    if (ref_line)
                    {
                        if (ref_line->type != DisplayLine::Type::Image)
                        {
                            throw std::runtime_error("ImageRefLine points to non image");
                        }
                        image_line = static_cast<const ImageLine *>(ref_line);
                    }
                }
                else
                {
                    image_line = static_cast<const ImageLine *>(line);
                }

                if (image_line)
                {
                    auto *surface = state->line_scroller.load_scaled_image(image_line->image_path);

                    // Amount of line height not used by image
                    uint32_t img_excess_y = image_line->num_lines * line_height - image_line->height;
                    // Y coordinate of image in screen space
                    int screen_start_y = line_y + img_excess_y / 2 - line_height * line_offset;

                    // Crop off-screen part of image. Allow to extend to edge of screen.
                    Sint16 src_y = std::max(-screen_start_y, 0);
                    Sint16 dst_y = std::max(screen_start_y, 0);

                    if (surface && src_y < (Sint16)image_line->height)
                    {
                        Uint16 width = image_line->width;
                        Uint16 height = image_line->height - src_y;

                        // Crop bottom
                        auto dst_y_bottom = dst_y + height;
                        Uint16 y_limit = state->line_pxl_limit_y();
                        if (dst_y_bottom > y_limit)
                        {
                            height -= dst_y_bottom - y_limit;
                        }

                        SDL_Rect src_rect = {0, src_y, width, height};
                        SDL_Rect dest_rect = {
                            static_cast<Sint16>((SCREEN_WIDTH - width) / 2),
                            dst_y,
                            0,
                            0
                        };
                        SDL_BlitSurface(surface, &src_rect, dest_surface, &dest_rect);
                    }
                }
            }
        }
        else
        {
            break;
        }

        line_y += line_height;
    }

    if (state->token_view_styling.get_show_title_bar())
    {
        // Recompute for short book case
        line_y = padding_y + num_text_display_lines * line_height;
        SDL_Rect title_crop_rect = {0, 0, 0, (Uint16)line_height};

        // Progress
        {
            char percent_str[32];
            snprintf(percent_str, sizeof(percent_str), " %d%%", state->title_progress_percent);

            SDL_Surface *page_surface = TTF_RenderUTF8_Shaded(font, percent_str, theme.secondary_text, theme.background);

            SDL_Rect dest_rect = {
                static_cast<Sint16>(SCREEN_WIDTH - page_surface->w - line_padding),
                static_cast<Sint16>(line_y + line_padding / 2),
                0, 0
            };
            title_crop_rect.w = SCREEN_WIDTH - line_padding * 2 - page_surface->w;

            SDL_BlitSurface(page_surface, nullptr, dest_surface, &dest_rect);
            SDL_FreeSurface(page_surface);
        }

        // Toc item
        if (state->title.size() > 0)
        {
            SDL_Rect dest_rect = {
                static_cast<Sint16>(line_padding),
                static_cast<Sint16>(line_y + line_padding / 2),
                0, 0
            };
            SDL_Surface *surface = TTF_RenderUTF8_Shaded(font, state->title.c_str(), theme.secondary_text, theme.background);
            SDL_BlitSurface(surface, &title_crop_rect, dest_surface, &dest_rect);
            SDL_FreeSurface(surface);
        }
    }

    return true;
}

// Adjust scroll amount to avoid going beyond start or end of book.
static int get_bounded_scroll_amount(TokenLineScroller &line_scroller, int num_display_lines, int num_lines)
{
    {
        // if start/end of book is within reach, make sure it is discovered
        line_scroller.get_line_relative(num_display_lines);
        line_scroller.get_line_relative(-num_display_lines);
    }

    int cur_line = line_scroller.get_line_number();
    int new_line = cur_line + num_lines;

    auto end_line = line_scroller.end_line_number();
    if (end_line)
    {
        new_line = std::min(
            *end_line - num_display_lines,
            new_line
        );
    }

    auto first_line = line_scroller.first_line_number();
    if (first_line)
    {
        new_line = std::max(
            *first_line,
            new_line
        );
    }

    return new_line - cur_line;
}

void TokenView::scroll(int num_lines)
{
    num_lines = get_bounded_scroll_amount(
        state->line_scroller,
        state->num_text_display_lines(),
        num_lines
    );
    if (num_lines != 0)
    {
        state->needs_render = true;
        state->line_scroller.seek_lines_relative(num_lines);
        if (state->on_scroll)
        {
            state->on_scroll(get_address());
        }
    }
}

static int word_index(const TokenViewState &state, DocAddr begin)
{
    for (int i = 0; i < static_cast<int>(state.page_words.size()); ++i)
    {
        if (state.page_words[i].begin == begin)
        {
            return i;
        }
    }
    return -1;
}

static int closest_on_line(const std::vector<LayoutWord> &words, int line_index, int x)
{
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < static_cast<int>(words.size()); ++i)
    {
        if (words[i].line_index != line_index)
        {
            continue;
        }
        int mid = words[i].x + words[i].w / 2;
        int dist = mid > x ? mid - x : x - mid;
        if (best < 0 || dist < best_dist)
        {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

void TokenView::on_keypress(SDLKey key)
{
    if (state->selecting)
    {
        if (key == SW_BTN_UP || key == SW_BTN_DOWN || key == SW_BTN_LEFT || key == SW_BTN_RIGHT)
        {
            move_word_selection(key);
            return;
        }
    }

    switch (key) {
        case SW_BTN_UP:
            scroll(-1);
            break;
        case SW_BTN_DOWN:
            scroll(1);
            break;
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            {
                auto [l_key, r_key] = get_shoulder_keymap_lr(
                    state->sys_styling.get_shoulder_keymap()
                );

                if (key == l_key)
                {
                    key = SW_BTN_LEFT;
                }
                else if (key == r_key)
                {
                    key = SW_BTN_RIGHT;
                }
            }
            // fallthrough
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            if (key == SW_BTN_LEFT)
            {
                scroll(-state->num_text_display_lines());
            }
            else if (key == SW_BTN_RIGHT)
            {
                scroll(state->num_text_display_lines());
            }
            break;
        default:
            break;
    }

    if (state->selecting)
    {
        layout_visible_words(*state);
        if (word_index(*state, state->selected_begin) < 0 && !state->page_words.empty())
        {
            state->selected_begin = state->page_words.front().begin;
            state->needs_render = true;
        }
    }
}

void TokenView::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    if (state->selecting && (key == SW_BTN_UP || key == SW_BTN_DOWN || key == SW_BTN_LEFT || key == SW_BTN_RIGHT))
    {
        if (state->word_move_throttle(held_time_ms))
        {
            on_keypress(key);
        }
        return;
    }

    switch (key) {
        case SW_BTN_UP:
        case SW_BTN_DOWN:
            if (state->line_scroll_throttle(held_time_ms))
            {
                on_keypress(key);
            }
            break;
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            if (state->page_scroll_throttle(held_time_ms))
            {
                on_keypress(key);
            }
            break;
        default:
            break;
    }
}

bool TokenView::is_done()
{
    return false;
}

DocAddr TokenView::get_address() const
{
    const DisplayLine *line = state->line_scroller.get_line_relative(0);
    if (line)
    {
        return line->address;
    }
    return 0;
}

void TokenView::seek_to_address(DocAddr address)
{
    state->line_scroller.seek_to_address(address);
    state->needs_render = true;
}

void TokenView::set_title(const std::string &title)
{
    if (title != state->title)
    {
        state->title = title;
        state->needs_render = true;
    }
}

void TokenView::set_title_progress(int percent)
{
    if (percent != state->title_progress_percent)
    {
        state->title_progress_percent = percent;
        state->needs_render = true;
    }
}

void TokenView::set_on_scroll(std::function<void(DocAddr)> callback)
{
    state->on_scroll = callback;
}

bool TokenView::is_word_selecting() const
{
    return state->selecting;
}

void TokenView::start_word_select()
{
    layout_visible_words(*state);
    if (state->page_words.empty())
    {
        return;
    }
    state->selecting = true;
    state->has_selection = true;
    state->selected_begin = state->page_words[0].begin;
    state->needs_render = true;
}

void TokenView::cancel_word_select()
{
    state->selecting = false;
    state->has_selection = false;
    state->needs_render = true;
}

std::string TokenView::selected_lookup() const
{
    int index = word_index(*state, state->selected_begin);
    if (index < 0)
    {
        return "";
    }
    return state->page_words[index].lookup;
}

bool TokenView::selected_is_highlighted() const
{
    return highlights_cover(state->highlights, state->selected_begin);
}

void TokenView::toggle_selected_highlight()
{
    int index = word_index(*state, state->selected_begin);
    if (index < 0)
    {
        return;
    }
    const LayoutWord &word = state->page_words[index];
    toggle_highlight(state->highlights, word.begin, word.end);
    state->remember_highlights();
    state->needs_render = true;
}

void TokenView::move_word_selection(SDLKey key)
{
    layout_visible_words(*state);
    if (state->page_words.empty())
    {
        return;
    }

    int index = word_index(*state, state->selected_begin);
    if (index < 0)
    {
        index = 0;
    }
    const LayoutWord &current = state->page_words[index];

    auto select_index = [&](int next) {
        if (next >= 0 && next < static_cast<int>(state->page_words.size()))
        {
            state->selected_begin = state->page_words[next].begin;
            state->has_selection = true;
            state->needs_render = true;
        }
    };

    if (key == SW_BTN_RIGHT)
    {
        if (index + 1 < static_cast<int>(state->page_words.size()))
        {
            select_index(index + 1);
            return;
        }
        DocAddr previous = current.begin;
        scroll(1);
        layout_visible_words(*state);
        int next = -1;
        for (int i = 0; i < static_cast<int>(state->page_words.size()); ++i)
        {
            if (state->page_words[i].begin > previous)
            {
                next = i;
                break;
            }
        }
        select_index(next < 0 ? static_cast<int>(state->page_words.size()) - 1 : next);
        return;
    }

    if (key == SW_BTN_LEFT)
    {
        if (index > 0)
        {
            select_index(index - 1);
            return;
        }
        DocAddr previous = current.begin;
        scroll(-1);
        layout_visible_words(*state);
        int next = -1;
        for (int i = 0; i < static_cast<int>(state->page_words.size()); ++i)
        {
            if (state->page_words[i].begin < previous)
            {
                next = i;
            }
        }
        select_index(next < 0 ? 0 : next);
        return;
    }

    int direction = key == SW_BTN_UP ? -1 : 1;
    int target_line = current.line_index + direction;
    int x = current.x + current.w / 2;
    int next = closest_on_line(state->page_words, target_line, x);
    if (next >= 0)
    {
        select_index(next);
        return;
    }

    scroll(direction);
    layout_visible_words(*state);
    if (state->page_words.empty())
    {
        return;
    }
    int edge = state->page_words.front().line_index;
    if (direction > 0)
    {
        edge = state->page_words.back().line_index;
    }
    select_index(closest_on_line(state->page_words, edge, x));
}
