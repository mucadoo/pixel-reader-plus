#ifndef TOKEN_VIEW_H_
#define TOKEN_VIEW_H_

#include "reader/view.h"
#include "doc_api/doc_addr.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DocReader;
struct SystemStyling;
struct TokenViewState;
struct TokenViewStyling;

class TokenView: public View
{
    std::unique_ptr<TokenViewState> state;

    void scroll(int num_lines);

public:
    TokenView(
        std::shared_ptr<DocReader> reader,
        DocAddr address,
        SystemStyling &sys_styling,
        TokenViewStyling &token_view_styling,
        std::filesystem::path highlights_path
    );
    virtual ~TokenView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;

    DocAddr get_address() const;
    void seek_to_address(DocAddr address);

    void set_title(const std::string &title);
    void set_title_progress(int percent, uint32_t page, uint32_t pages);

    // Counts display lines for page mode. Returns true while more counting remains.
    bool advance_page_index(bool chapter);
    bool page_index_ready(bool chapter);
    bool page_numbers(bool chapter, uint32_t &page, uint32_t &pages);
    // Move to the first line of the page that contains the current line.
    // Returns true when the view actually moved.
    bool align_to_page(bool chapter);
    void turn_page(int direction);

    void set_on_scroll(std::function<void(DocAddr)> callback);

    bool is_word_selecting() const;
    void start_word_select();
    void cancel_word_select();
    void toggle_extend_selection();
    void move_word_selection(SDLKey key);
    std::string selected_lookup() const;
    bool selected_is_highlighted() const;
    void toggle_selected_highlight();
};

#endif
