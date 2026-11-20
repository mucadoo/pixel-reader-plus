#ifndef PAGE_INDEX_H_
#define PAGE_INDEX_H_

#include "doc_api/doc_addr.h"
#include "doc_api/token_iter.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TokenLineScroller;

// Display lines for one book or one chapter, in reading order.
// Page starts are computed from these lines, so a chapter or a picture
// can begin at the top of a page instead of on a fixed grid.
class PageIndex
{
    struct Slot
    {
        DocAddr address = 0;
        uint16_t group = 0;
        bool hard_break = false;
        bool blank = false;
    };

    std::shared_ptr<TokenIter> it;
    std::optional<DocAddr> end_address;
    std::string key;
    std::vector<Slot> slots;
    bool running = false;
    bool complete = false;
    bool have_chapter = false;
    uint32_t last_chapter = 0;
    bool pending_break = false;

    mutable int cached_page_size = 0;
    mutable std::vector<int> page_starts;

    void ensure_pages(int page_size) const;

public:
    bool matches(const std::string &key) const;
    bool ready_for(const std::string &key) const;
    void start(std::shared_ptr<TokenIter> it, std::optional<DocAddr> end, std::string key);
    // True while more tokens remain.
    bool pump(TokenLineScroller &scroller, int budget);
    int total_lines() const;
    // prefer_blank selects a blank line that shares its address with the following text.
    int line_for_address(DocAddr address, bool prefer_blank) const;
    int page_count(int lines_per_page) const;
    int page_of_line(int line, int lines_per_page) const;
    int first_line_of_page(int page, int lines_per_page) const;
};

#endif
