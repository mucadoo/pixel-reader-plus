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

class Stylesheet
{
public:
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
