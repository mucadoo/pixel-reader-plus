#ifndef TOKEN_VIEW_H_
#define TOKEN_VIEW_H_

#include "reader/view.h"
#include "doc_api/doc_addr.h"
#include "reader/highlights.h"

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

    // Marks the words of a search on the page, and puts the note (such as "Match 3 / 17") before the
    // chapter title in the bar. An empty query and note clear them.
    void set_search_marks(const std::string &query, const std::string &note);
    // The printed page number to show in the bar in place of the counted one. Empty for none.
    void set_page_label(const std::string &label);
    // Where the bookmarks are, for the corner mark on a page that holds one.
    void set_marks(std::vector<DocAddr> marks);
    // The last line of the book is on screen.
    bool at_end() const;

    DocAddr get_address() const;
    void seek_to_address(DocAddr address);

    void set_title(const std::string &title);
    void set_book_title(const std::string &title);
    // percent follows the Progress setting; the other two are always given
    // Where the chapter and page counters of the bar stand. Pages left are -1 when not known.
    void set_bar_extras(int chapter_index, int chapter_count, int chapter_pages_left, int book_pages_left);
    // The page of the book and of the chapter, counted from the pages of the whole book. -1 when unknown.
    void set_bar_pages(int book_page, int book_pages, int chapter_page, int chapter_pages);
    // The page within the current chapter and the pages in it, from the page count of the book.
    bool chapter_page_numbers(uint32_t &page, uint32_t &pages);
    void set_title_progress(int percent, uint32_t page, uint32_t pages, int chapter_percent, int book_percent);
    // Redraws the bar when the clock in it changes minute.
    bool tick(uint32_t elapsed_ms) override;

    // Counts display lines for page mode. Returns true while more counting remains.
    bool advance_page_index(bool chapter);
    bool page_index_ready(bool chapter);
    // How far the running count has got, 0 to 100.
    int page_count_percent(bool chapter);
    bool page_numbers(bool chapter, uint32_t &page, uint32_t &pages);
    // Move to the first line of the page that contains the current line.
    // Returns true when the view actually moved.
    bool align_to_page(bool chapter);
    void turn_page(int direction);
    // Draw the page again on the next frame, after something else used the screen.
    void invalidate();
    // The dictionary name of the book language, which picks the hyphenation patterns.
    void set_hyphenation_language(const std::string &language);
    // Moves to the top of the given page (counting from 1). False when the count is not ready.
    bool go_to_page(bool chapter, uint32_t page);

    void set_on_scroll(std::function<void(DocAddr)> callback);

    bool is_word_selecting() const;
    void start_word_select();
    void cancel_word_select();
    void toggle_extend_selection();
    void move_word_selection(SDLKey key);
    std::string selected_lookup() const;
    // The link the first selected word belongs to, or null.
    std::shared_ptr<const std::string> selected_link() const;
    bool selected_is_highlighted() const;
    void toggle_selected_highlight(HighlightStyle style = HighlightStyle::Marker);
    // The words between the first and last selected word, as they were on the pages.
    std::string selected_text() const;
    // The sentence on the page around the selected word, empty when there is none.
    std::string selected_sentence() const;
    // X in word selection: the selection grows to the sentence, then the paragraph, then back to the word.
    void grow_selection();

    // Paths of the pictures that show on the screen now, top to bottom.
    std::vector<std::filesystem::path> images_on_screen() const;

    const std::vector<Highlight> &highlights() const;
    void remove_highlight(size_t index);
    void set_highlight_note(size_t index, const std::string &note);
    void set_highlight_style(size_t index, HighlightStyle style);
    // Adds the highlights that are not there yet, and fills in notes that are missing. Returns how many changed.
    size_t merge_highlights(const std::vector<Highlight> &incoming);
    // The addresses the page on screen covers: from the first line up to the next page. false when nothing is shown.
    bool visible_range(DocAddr &from, DocAddr &until) const;
};

#endif
