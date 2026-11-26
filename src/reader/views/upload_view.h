#ifndef UPLOAD_VIEW_H_
#define UPLOAD_VIEW_H_

#include "reader/view.h"
#include "reader/wifi_upload.h"

#include <filesystem>
#include <functional>
#include <string>

struct SystemStyling;

// The screen shown while the handheld takes books over wifi. B stops it.
class UploadView: public View
{
    bool _is_done = false;
    bool _needs_render = true;
    UploadServer server;
    std::string address;   // what to type in the browser, or empty
    std::string problem;   // why it could not start
    std::filesystem::path folder;
    std::function<void()> on_finished;
    SystemStyling &styling;
    uint32_t styling_sub_id;

public:
    UploadView(std::filesystem::path folder, SystemStyling &styling, std::function<void()> on_finished);
    virtual ~UploadView();

    bool render(SDL_Surface *dest, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
    bool tick(uint32_t elapsed_ms) override;
    void on_pop() override;
};

#endif