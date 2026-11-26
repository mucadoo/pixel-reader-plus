#include "./token_line_scroller.h"

#include "doc_api/token_addressing.h"
#include "reader/hyphenation.h"
#include "util/picture_filter.h"
#include "reader/text_style.h"
#include "reader/text_wrap.h"
#include "sys/screen.h"
#include "util/sdl_utils.h"
#include "util/str_utils.h"

#include "extern/rotozoom/SDL_rotozoom.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace
{

const std::string BULLET = "•";

uint32_t get_line_for_address(const IndexedDequeue<std::unique_ptr<DisplayLine>> &lines, DocAddr address)
{
    int best_line = lines.start_index();
    for (int i = lines.start_index(); i < lines.end_index(); ++i)
    {
        const auto &line = lines[i];
        if (line && line->continuation)
        {
            continue;
        }
        if (line && line->address <= address)
        {
            best_line = i;
            if (line->address == address)
            {
                // A blank line between paragraphs has the address of the text after it. A place saved
                // at the top of a page means the text, not the blank above it: starting on the blank
                // puts it on the previous page.
                for (int j = i; j < lines.end_index(); ++j)
                {
                    const auto &next = lines[j];
                    if (!next || next->address != address)
                    {
                        break;
                    }
                    if (next->continuation)
                    {
                        continue;
                    }
                    const bool blank = next->type == DisplayLine::Type::Text
                        && static_cast<const TextLine *>(next.get())->text.empty();
                    if (!blank)
                    {
                        best_line = j;
                        break;
                    }
                }
                break;
            }
        }
        else
        {
            break;
        }
    }

    return best_line;
}

float scale_to_fit_width(int w)
{
    if (w > SCREEN_WIDTH)
    {
        return SCREEN_WIDTH / static_cast<float>(w);
    }
    return 1;
}

} // namespace

// The scale a picture is shown at: it fits the width of the screen.
float TokenLineScroller::image_scale(int w, int) const
{
    return scale_to_fit_width(w);
}

std::string TokenLineScroller::image_key(const std::filesystem::path &path) const
{
    return path.string() + "#" + std::to_string(picture_mode);
}

std::vector<std::unique_ptr<DisplayLine>> TokenLineScroller::image_to_display_lines(const ImageDocToken &token)
{
    SDL_Surface *image = load_scaled_image(token.path);

    std::vector<std::unique_ptr<DisplayLine>> lines;
    if (image && image->h)
    {
        int num_lines = (image->h + line_height_pixels - 1) / line_height_pixels;
        lines.emplace_back(std::make_unique<ImageLine>(token.address, token.path, num_lines, image->w, image->h));
        for (int i = 1; i < num_lines; ++i)
        {
            auto ref = std::make_unique<ImageRefLine>(token.address, i);
            ref->continuation = true;
            lines.emplace_back(std::move(ref));
        }
    }
    else
    {
        // Fallback for error loading image
        lines.emplace_back(std::make_unique<TextLine>(token.address, ""));
        lines.emplace_back(std::make_unique<TextLine>(token.address, "[Image " + token.path.string() + "]"));
        lines.emplace_back(std::make_unique<TextLine>(token.address, ""));
    }
    return lines;
}


std::vector<std::unique_ptr<DisplayLine>> TokenLineScroller::render_display_lines(const DocToken &token)
{
    if (token.type == TokenType::Image)
    {
        if (picture_mode == PICTURES_HIDDEN)
        {
            return {};
        }
        return image_to_display_lines(static_cast<const ImageDocToken &>(token));
    }
    else
    {
        std::string text;
        uint32_t extra_text_width = 0;

        if (token.type == TokenType::Text)
        {
            text = static_cast<const TextDocToken &>(token).text;
        }
        else if (token.type == TokenType::ListItem)
        {
            const ListItemDocToken &list_token = static_cast<const ListItemDocToken &>(token);
            int nest_level = list_token.nest_level;
            std::string prefix = std::string(
                (nest_level > 1 ? nest_level - 1 : 0) * 2,
                ' '
            ) + BULLET + " ";
            text = prefix + list_token.text;
            extra_text_width = get_address_width(prefix);
        }
        else if (token.type == TokenType::Header)
        {
            text = static_cast<const HeaderDocToken &>(token).text;
        }
        else
        {
            throw std::runtime_error("Unknown token type");
        }

        const int header_level = token.type == TokenType::Header
            ? (token.header_level > 0 ? token.header_level : 1)
            : 0;
        const bool pre = token.pre;
        const bool quote = token.quote;
        float scale = token.font_scale > 0.2f ? token.font_scale : 1.f;
        if (!token.scale_set && header_level > 0)
        {
            scale = header_level == 1 ? 1.5f : header_level == 2 ? 1.25f : 1.15f;
        }
        const bool base_bold = token.bold || header_level > 0 || (bold_body && !pre);
        const bool base_italic = token.italic || quote;
        const bool centered = token.centered || (!token.center_set && header_level > 0 && header_level <= 2);
        const int indent_px = (token.type == TokenType::Text && !pre && !centered && !quote && !text.empty())
            ? first_line_indent
            : 0;
        if (!paragraph_gap && token.type == TokenType::Text && text.empty() && !pre)
        {
            return {}; // the blank line between paragraphs is switched off
        }
        const uint32_t span_shift = static_cast<uint32_t>(text.size() - (
            token.type == TokenType::ListItem
                ? static_cast<const ListItemDocToken &>(token).text.size()
                : token.type == TokenType::Header
                    ? static_cast<const HeaderDocToken &>(token).text.size()
                    : static_cast<const TextDocToken &>(token).text.size()
        ));
        const std::vector<StyleSpan> shifted_spans = [&]() {
            std::vector<StyleSpan> shifted = token.spans;
            for (StyleSpan &span : shifted)
            {
                span.start += span_shift;
                span.end += span_shift;
            }
            return shifted;
        }();

        auto font_for = [&](int style, float run_scale) -> TTF_Font * {
            if (!style_font)
            {
                return nullptr;
            }
            return style_font(run_scale, pre, style);
        };

        const char *full = text.c_str();
        auto fits = [&](const char *s, uint32_t len) {
            if (!style_font)
            {
                return line_fits(s, len);
            }
            int avail = SCREEN_WIDTH - side_margin - (quote ? 18 : 0) - (s == full ? indent_px : 0);
            int width = measure_styled(
                full,
                static_cast<uint32_t>(s - full),
                len,
                shifted_spans,
                base_bold,
                base_italic,
                scale,
                font_for
            );
            return width <= avail;
        };

        auto slots_for = [&](float run_scale) {
            int slot_span = 1;
            if (style_font && (run_scale > 1.08f || pre))
            {
                TTF_Font *sample = style_font(run_scale, pre, style_flags(base_bold, base_italic));
                int glyph_h = sample ? detect_line_height(sample) : static_cast<int>(line_height_pixels);
                slot_span = std::max(1, (glyph_h + 4 + static_cast<int>(line_height_pixels) - 1) / static_cast<int>(line_height_pixels));
                if (run_scale >= 1.5f)
                {
                    slot_span = std::max(slot_span, 2);
                }
            }
            return slot_span;
        };

        DocAddr address = token.address;
        std::vector<std::unique_ptr<DisplayLine>> lines;
        auto on_line = [&](const char *str, uint32_t len, bool hyphenated) {
            std::string line_text(str, len);
            // The hyphen is drawn but is not part of the book text, so it takes no address.
            auto line = std::make_unique<TextLine>(address, hyphenated ? line_text + "-" : line_text, centered);
            line->hyphenated = hyphenated;
            line->header_level = header_level;
            line->font_scale = scale;
            line->bold = base_bold;
            line->italic = base_italic;
            line->pre = pre;
            line->quote = quote;
            line->first_indent = lines.empty() ? indent_px : 0;
            line->address_lead = lines.empty() ? extra_text_width : 0;
            line->spans = clip_spans(
                token.spans,
                span_shift,
                static_cast<uint32_t>(str - full),
                len
            );
            float line_scale = scale;
            for (const StyleSpan &span : line->spans)
            {
                if (span.scale > line_scale)
                {
                    line_scale = span.scale;
                }
            }
            line->slot_span = line_text.empty() ? 1 : slots_for(line_scale);
            int used_span = line->slot_span;
            lines.push_back(std::move(line));
            for (int extra = 1; extra < used_span; ++extra)
            {
                auto continuation = std::make_unique<TextLine>(address, "");
                continuation->continuation = true;
                lines.push_back(std::move(continuation));
            }

            address += get_address_width(line_text);
            if (lines.size() == static_cast<size_t>(used_span))
            {
                address -= extra_text_width;
            }
        };
        if (hyphenator && hyphenator->ready() && style_font && !pre && header_level == 0 && !centered)
        {
            const int avail = SCREEN_WIDTH - side_margin - (quote ? 18 : 0);
            const int hyphen_w = measure_styled("-", 0, 1, {}, base_bold, base_italic, scale, font_for);
            wrap_lines_hyphenated(
                full,
                fits,
                [&](const char *s, uint32_t len) {
                    const int line_avail = avail - (s == full ? indent_px : 0);
                    return measure_styled(
                               full,
                               static_cast<uint32_t>(s - full),
                               len,
                               shifted_spans,
                               base_bold,
                               base_italic,
                               scale,
                               font_for
                           ) + hyphen_w <= line_avail;
                },
                [this](const char *word, uint32_t len) {
                    return hyphenator->points(std::string(word, len));
                },
                on_line
            );
        }
        else
        {
            wrap_lines(full, fits, [&](const char *str, uint32_t len) { on_line(str, len, false); });
        }

        for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        {
            if (!(*it)->continuation)
            {
                static_cast<TextLine *>(it->get())->paragraph_end = true;
                break;
            }
        }
        return lines;
    }
}

void TokenLineScroller::collect_token_lines(const DocToken &token, std::vector<DocAddr> &line_addresses)
{
    if (token.type == TokenType::Image)
    {
        const auto &image_token = static_cast<const ImageDocToken &>(token);
        if (picture_mode == PICTURES_HIDDEN)
        {
            return;
        }
        int slots = 3;
        if (SDL_Surface *cached = image_cache.get_image(image_key(image_token.path)))
        {
            if (cached->h > 0)
            {
                slots = std::max(1, (cached->h + static_cast<int>(line_height_pixels) - 1) / static_cast<int>(line_height_pixels));
            }
        }
        else
        {
            auto data = reader->load_resource(image_token.path);
            int w = 0;
            int h = 0;
            if (!data.empty() && image_pixel_size(data.data(), static_cast<uint32_t>(data.size()), w, h))
            {
                float scale = image_scale(w, h);
                int scaled_h = std::max(1, static_cast<int>(h * scale));
                slots = std::max(1, (scaled_h + static_cast<int>(line_height_pixels) - 1) / static_cast<int>(line_height_pixels));
            }
        }
        for (int i = 0; i < slots; ++i)
        {
            line_addresses.push_back(token.address);
        }
        return;
    }

    for (const auto &line : render_display_lines(token))
    {
        line_addresses.push_back(line->address);
    }
}

void TokenLineScroller::get_more_lines_forward(uint32_t num_lines)
{
    while (num_lines > 0)
    {
        const DocToken *token = forward_it->read(1);
        if (!token)
        {
            global_end_line = lines_buf.end_index();
            break;
        }

        for (auto &line : render_display_lines(*token))
        {
            lines_buf.append(std::move(line));
            if (num_lines > 0)
            {
                --num_lines;
            }
        }
    }
}

void TokenLineScroller::get_more_lines_backward(uint32_t num_lines)
{
    while (num_lines > 0)
    {
        const DocToken *token = backward_it->read(-1);
        if (!token)
        {
            global_first_line = lines_buf.start_index();
            break;
        }

        std::vector<std::unique_ptr<DisplayLine>> lines = render_display_lines(*token);
        for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        {
            lines_buf.prepend(std::move(*it));
            if (num_lines > 0)
            {
                --num_lines;
            }
        }
    }
}

void TokenLineScroller::clear_buffer()
{
    lines_buf.clear();
    current_line = 0;
    global_first_line = std::nullopt;
    global_end_line = std::nullopt;
}

void TokenLineScroller::initialize_buffer_at(DocAddr address)
{
    clear_buffer();

    backward_it = reader->get_iter(address);
    forward_it = backward_it->clone();

    materialize_line(0);
    materialize_line(8); // far enough to see the text behind blank lines that share the address
    current_line = get_line_for_address(lines_buf, address);
}

TokenLineScroller::TokenLineScroller(
    const std::shared_ptr<DocReader> reader,
    DocAddr address,
    std::function<bool(const char *, uint32_t)> line_fits,
    uint32_t line_height_pixels,
    StyleFontFn style_font,
    int side_margin
) : reader(reader),
    forward_it(nullptr),
    backward_it(nullptr),
    line_fits(line_fits),
    style_font(std::move(style_font)),
    line_height_pixels(line_height_pixels),
    side_margin(side_margin)
{
    initialize_buffer_at(address);
}

void TokenLineScroller::materialize_line(int line_num)
{
    int forward_needed = line_num - lines_buf.end_index() + 1;
    if (forward_needed > 0)
    {
        get_more_lines_forward(forward_needed);
    }

    int backwards_lines_needed = lines_buf.start_index() - line_num;
    if (backwards_lines_needed > 0)
    {
        get_more_lines_backward(backwards_lines_needed);
    }
}

const DisplayLine *TokenLineScroller::get_line_relative(int offset)
{
    int line = current_line + offset;
    materialize_line(line);

    if (line < lines_buf.start_index() || line >= lines_buf.end_index())
    {
        return nullptr;
    }
    return lines_buf[line].get();
}

int TokenLineScroller::get_line_number() const
{
    return current_line;
}

void TokenLineScroller::seek_lines_relative(int offset)
{
    current_line += offset;
    materialize_line(current_line);
}

void TokenLineScroller::seek_to_address(DocAddr address)
{
    initialize_buffer_at(address);
}

void TokenLineScroller::reset_buffer()
{
    const DisplayLine *line = get_line_relative(0);
    if (line)
    {
        DocAddr cur_address = line->address;
        clear_buffer();
        initialize_buffer_at(cur_address);
    }
}

void TokenLineScroller::set_line_height_pixels(uint32_t new_height)
{
    line_height_pixels = new_height;
}

void TokenLineScroller::set_picture_mode(int mode)
{
    picture_mode = mode;
}

void TokenLineScroller::set_text_options(int first_line_indent_pixels, bool blank_between_paragraphs, bool bold_text)
{
    first_line_indent = first_line_indent_pixels;
    paragraph_gap = blank_between_paragraphs;
    bold_body = bold_text;
}

void TokenLineScroller::set_hyphenator(const Hyphenator *new_hyphenator)
{
    hyphenator = new_hyphenator;
}

void TokenLineScroller::set_side_margin(int pixels)
{
    side_margin = pixels;
}

std::optional<int> TokenLineScroller::first_line_number() const
{
    return global_first_line;
}

std::optional<int> TokenLineScroller::end_line_number() const
{
    return global_end_line;
}

SDL_Surface *TokenLineScroller::load_scaled_image(const std::filesystem::path &path)
{
    {
        SDL_Surface *image = image_cache.get_image(image_key(path));
        if (image)
        {
            return image;
        }
    }

    auto img_data = reader->load_resource(path);
    if (img_data.empty())
    {
        std::cerr << "Failed to read image data: " << path << std::endl;
        return nullptr;
    }

    std::string file_ext;
    try
    {
        file_ext = path.extension().string().substr(1);
    }
    catch (const std::out_of_range &)
    {
        return nullptr;
    }

    auto img_surface = load_surface_from_ptr(
        img_data.data(),
        img_data.size(),
        file_ext.c_str(),
        get_render_surface_format()
    );
    if (!img_surface)
    {
        std::cerr << "Failed to load image: " << path << std::endl;
        return nullptr;
    }

    float scale = image_scale(img_surface->w, img_surface->h);
    surface_unique_ptr shown = scale != 1
        ? surface_unique_ptr { zoomSurface(img_surface.get(), scale, scale, 0) }
        : std::move(img_surface);
    apply_picture_filter(shown.get(), picture_mode);
    const std::string key = image_key(path);
    image_cache.put_image(key, std::move(shown));

    return image_cache.get_image(key);
}
