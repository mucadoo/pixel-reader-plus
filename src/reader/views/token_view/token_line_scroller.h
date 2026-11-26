#ifndef TOKEN_LINE_SCROLLER_H_
#define TOKEN_LINE_SCROLLER_H_

#include "./display_line.h"

#include <SDL/SDL_ttf.h>

#include "doc_api/doc_addr.h"
#include "doc_api/doc_reader.h"
#include "doc_api/doc_token.h"
#include "util/indexed_dequeue.h"
#include "util/sdl_image_cache.h"
#include "util/sdl_pointer.h"

#include <functional>
#include <optional>

using StyleFontFn = std::function<TTF_Font *(float scale, bool pre, int style)>;

// Lazy renders tokens into lines of text, and provides access to the lines
// through an infinite-scroll type interface.
class Hyphenator;

class TokenLineScroller
{
    const std::shared_ptr<DocReader> reader;
    std::shared_ptr<TokenIter> forward_it;
    std::shared_ptr<TokenIter> backward_it;
    std::function<bool(const char *, uint32_t)> line_fits;
    StyleFontFn style_font;

    std::optional<int> global_first_line;
    std::optional<int> global_end_line;

    uint32_t line_height_pixels;
    int side_margin = 4;
    int picture_mode = 0;
    int first_line_indent = 0;
    bool paragraph_gap = true;
    bool bold_body = false;
    const Hyphenator *hyphenator = nullptr;
    int current_line = 0;

    IndexedDequeue<std::unique_ptr<DisplayLine>> lines_buf;
    SDLImageCache image_cache;

    std::vector<std::unique_ptr<DisplayLine>> image_to_display_lines(const ImageDocToken &token);
    std::vector<std::unique_ptr<DisplayLine>> render_display_lines(const DocToken &token);

    void get_more_lines_forward(uint32_t num);
    void get_more_lines_backward(uint32_t num);
    void clear_buffer();
    void initialize_buffer_at(DocAddr address);
    void materialize_line(int line_num);

public:
    TokenLineScroller(
        const std::shared_ptr<DocReader> reader,
        DocAddr address,
        std::function<bool(const char *, uint32_t)> line_fits,
        uint32_t line_height_pixels,
        StyleFontFn style_font,
        int side_margin = 4
    );

    void collect_token_lines(const DocToken &token, std::vector<DocAddr> &line_addresses);

    const DisplayLine *get_line_relative(int offset);
    int get_line_number() const;
    void seek_lines_relative(int offset);
    void seek_to_address(DocAddr address);
    void reset_buffer();
    void set_line_height_pixels(uint32_t line_height_pixels);
    // Space left empty at each side of the text. Call reset_buffer to wrap again.
    void set_side_margin(int pixels);
    // The factor a picture of this size is drawn at.
    float image_scale(int w, int h) const;
    // First-line indent in pixels, whether blank lines separate paragraphs, and bold body text.
    // Call reset_buffer to wrap again.
    // How pictures are shown, a PictureMode. Call reset_buffer to lay out again.
    void set_picture_mode(int mode);
    void set_text_options(int first_line_indent_pixels, bool blank_between_paragraphs, bool bold_text);
    // Words may be split at the end of a line, with these patterns. Null for never.
    // Call reset_buffer to wrap again.
    void set_hyphenator(const Hyphenator *hyphenator);

    std::optional<int> first_line_number() const;
    std::optional<int> end_line_number() const;

    SDL_Surface *load_scaled_image(const std::filesystem::path &path);
};

#endif
