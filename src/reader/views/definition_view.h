#ifndef DEFINITION_VIEW_H_
#define DEFINITION_VIEW_H_

#include "reader/view.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SystemStyling;

class DefinitionView: public View
{
    bool _is_done = false;
    bool _needs_render = true;
    uint32_t scroll_pos = 0;
    std::string title;
    std::string body;
    std::string font_name;
    SystemStyling &styling;
    uint32_t styling_sub_id;

    // Looking up a word of the entry: A starts picking a word, A again looks it up.
    std::function<void(const std::string &)> on_lookup;
    struct WordPos
    {
        uint32_t line;
        uint32_t begin; // bytes within the line
        uint32_t end;
    };
    std::vector<std::string> lines;
    std::vector<WordPos> words;
    // Flash card mode: the meaning stays hidden until A. Then A means known and Y means not yet.
    // The callback gets the answer and returns the next card, if there is one.
    using CardNext = std::function<std::optional<std::pair<std::string, std::string>>(bool)>;
    CardNext card_next;
    std::string card_body;
    bool revealed = false;
    bool picking = false;
    uint32_t picked = 0;
    uint32_t visible_lines = 1;

    void build_layout();
    int word_x(uint32_t index) const;
    void move_pick(SDLKey key);

public:
    DefinitionView(std::string title, std::string body, SystemStyling &styling);
    // With a callback the entry can be read word by word: a picked word is passed to it.
    // The meaning is shown only after A.
    void set_card(std::string meaning, CardNext next)
    {
        card_body = std::move(meaning);
        card_next = std::move(next);
        body.clear();
    }
    void set_lookup(std::function<void(const std::string &)> callback) { on_lookup = std::move(callback); }
    virtual ~DefinitionView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;
};

#endif
