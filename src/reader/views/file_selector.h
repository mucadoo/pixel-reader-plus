#ifndef FILE_SELECTOR_H_
#define FILE_SELECTOR_H_

#include "reader/browser_sort.h"
#include "reader/view.h"

#include <SDL/SDL_video.h>

#include <functional>
#include <filesystem>
#include <memory>
#include <string>

struct FSState;
struct SystemStyling;

// The book browser. It shows the folder as a list or as a grid of covers.
class FileSelector: public View
{
    std::unique_ptr<FSState> state;

public:
    // Expects to receive a path to a file, or directory with trailing separator.
    // When top_folder is given, the browser never goes above it.
    FileSelector(std::filesystem::path path, SystemStyling &styling, std::filesystem::path top_folder = {});
    virtual ~FileSelector();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
    void on_focus() override;
    bool tick(uint32_t elapsed_ms) override;

    void set_on_file_selected(std::function<void(const std::filesystem::path &)> on_file_selected);
    void set_on_file_focus(std::function<void(const std::filesystem::path &)> on_file_focus);
    void set_on_view_focus(std::function<void()> on_view_focus);
    // Called with a title and a text when the user asks for the properties of an entry.
    // Where menus the browser opens itself, such as the book options, are shown.
    void set_on_open_view(std::function<void(std::shared_ptr<View>)> on_open_view);
    // Leave finished books out of the list.
    void set_hide_finished(bool hide);
    void set_on_hide_finished_change(std::function<void(bool)> callback);
    // Called with the book id when a book is deleted, so the data kept for it can go too.
    void set_on_book_deleted(std::function<void(const std::string &book_id)> callback);
    void set_on_info(std::function<void(const std::string &, const std::string &)> on_info);

    // Where cover thumbnails are kept. Without it the grid shows plain covers only.
    void set_cover_cache_dir(std::filesystem::path dir);
    // Where the titles and authors read from epubs are kept. Without it the
    // browser shows file names.
    void set_info_cache_dir(std::filesystem::path dir);
    void set_sort(BrowserSort sort);
    void set_on_sort_change(std::function<void(BrowserSort)> callback);
    void set_grid_view(bool grid);
    bool grid_view() const;
    void set_on_view_mode_change(std::function<void(bool grid)> callback);
};

#endif
