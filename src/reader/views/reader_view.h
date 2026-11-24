#ifndef READER_VIEW_H_
#define READER_VIEW_H_

#include "doc_api/doc_addr.h"
#include "reader/view.h"

#include <filesystem>
#include <functional>
#include <string>

struct DocReader;
struct ReaderViewState;
struct SystemStyling;
struct TokenViewStyling;
struct ViewStack;

class ReaderView: public View
{
    std::unique_ptr<ReaderViewState> state;

    void update_token_view_title(DocAddr address);
    bool step_pagination();
    // Count pages for up to budget_ms. True when something changed.
    bool run_pagination(uint32_t budget_ms);
    bool pagination_ready() const;
    // A key or a jump can send the view into a stretch that has not been counted.
    // Finish a short count before the next frame, so the screen never shows the
    // unaligned view first and snaps to the page a moment later.
    void settle_pagination(bool was_ready);
    void handle_keypress(SDLKey key);

public:
    ReaderView(
        std::filesystem::path path,
        std::shared_ptr<DocReader> reader,
        DocAddr seek_address,
        SystemStyling &sys_styling,
        TokenViewStyling &token_view_styling,
        ViewStack &view_stack,
        std::filesystem::path highlights_path
    );
    ReaderView(const ReaderView &) = delete;
    ReaderView &operator=(const ReaderView &) = delete;
    virtual ~ReaderView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;

    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t hold_time_ms) override;

    void set_on_change_address(std::function<void(DocAddr)> callback);

    void seek_to_toc_index(uint32_t toc_index);
    void seek_to_address(DocAddr address);
};

#endif
