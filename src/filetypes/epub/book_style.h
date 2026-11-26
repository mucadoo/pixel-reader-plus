#ifndef BOOK_STYLE_H_
#define BOOK_STYLE_H_

#include <string>
#include <vector>

// Visual style taken from a book's stylesheet. scale is relative to the
// user's font, where 1 is body text (treated as 12pt).
struct BookStyle
{
    float scale = 1.f;
    bool scale_set = false;
    bool bold = false;
    bool italic = false;
    bool center = false;
    bool center_set = false;
    // 0 stays on this page, 1 starts a new page. break_*_set means the stylesheet chose it.
    int break_before = 0;
    bool break_before_set = false;
    int break_after = 0;
    bool break_after_set = false;
};

// Whether the stylesheets of a book are used. Off leaves only the reader's own styling, with
// the book's headings still larger. Set before a book is opened.
void set_book_styles_enabled(bool enabled);
bool book_styles_enabled();

// Pieces of a book's styling that can be switched off one by one, with set_style_tweaks, before a book is
// opened. Smart quotes is a change to the text itself.
constexpr int TWEAK_PLAIN_BREAKS = 1;   // ignore the book's page breaks, and the new page at each h1
constexpr int TWEAK_PLAIN_SIZES = 2;    // ignore the book's font sizes
constexpr int TWEAK_PLAIN_CENTER = 4;   // ignore centering and left alignment
constexpr int TWEAK_SMART_QUOTES = 8;   // curly quotes in place of straight ones
constexpr int TWEAK_COUNT = 4;
void set_style_tweaks(int mask);
int style_tweaks();

class Stylesheet
{public:
    void parse(const std::string &css);
    BookStyle apply(
        const BookStyle &parent,
        const std::string &tag,
        const std::vector<std::string> &classes
    ) const;

private:
    struct Rule
    {
        std::string tag;
        std::string cls;
        int order = 0;
        int specificity = 0;
        float font_pt = -1.f;
        float font_em = -1.f;
        float font_pct = -1.f;
        int bold = -1;
        int italic = -1;
        int align = -1;
        int break_before = -1;
        int break_after = -1;
    };

    std::vector<Rule> rules;
};

#endif
