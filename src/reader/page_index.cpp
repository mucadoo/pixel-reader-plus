#include "./page_index.h"

#include "doc_api/doc_token.h"
#include "reader/views/token_view/token_line_scroller.h"

#include <algorithm>

namespace
{

uint32_t spine_chapter(DocAddr address)
{
    // Same packing as epub addresses: spine index in bits 32-47. Plain text stays 0.
    return static_cast<uint32_t>((address >> 32) & 0xFFFF);
}

bool token_is_blank(const DocToken &token)
{
    if (token.type == TokenType::Image)
    {
        return false;
    }
    if (token.type == TokenType::Header)
    {
        return static_cast<const HeaderDocToken &>(token).text.empty();
    }
    if (token.type == TokenType::ListItem)
    {
        return static_cast<const ListItemDocToken &>(token).text.empty();
    }
    if (token.type == TokenType::Text)
    {
        return static_cast<const TextDocToken &>(token).text.empty();
    }
    return false;
}

} // namespace

bool PageIndex::matches(const std::string &want) const
{
    return key == want && (running || complete);
}

bool PageIndex::ready_for(const std::string &want) const
{
    return complete && key == want;
}

void PageIndex::start(std::shared_ptr<TokenIter> iter, std::optional<DocAddr> end, std::string new_key)
{
    it = std::move(iter);
    end_address = end;
    key = std::move(new_key);
    slots.clear();
    slots.reserve(2048);
    running = true;
    complete = false;
    have_chapter = false;
    last_chapter = 0;
    pending_break = false;
    cached_page_size = 0;
    page_starts.clear();
}

bool PageIndex::pump(TokenLineScroller &scroller, int budget)
{
    if (!running)
    {
        return false;
    }

    for (int n = 0; n < budget; ++n)
    {
        if (!it)
        {
            running = false;
            complete = true;
            cached_page_size = 0;
            return false;
        }

        const DocToken *token = it->read(1);
        if (!token || (end_address && token->address >= *end_address))
        {
            running = false;
            complete = true;
            cached_page_size = 0;
            return false;
        }

        uint32_t chapter = spine_chapter(token->address);
        bool want_break = pending_break || token->page_break_before;
        if (!have_chapter)
        {
            have_chapter = true;
            last_chapter = chapter;
        }
        else if (chapter != last_chapter)
        {
            want_break = true;
            last_chapter = chapter;
        }

        std::vector<DocAddr> laid;
        scroller.collect_token_lines(*token, laid);
        if (laid.empty())
        {
            if (want_break)
            {
                pending_break = true;
            }
        }
        else if (token_is_blank(*token))
        {
            if (want_break)
            {
                pending_break = true;
            }
            for (DocAddr address : laid)
            {
                Slot slot;
                slot.address = address;
                slot.blank = true;
                slots.push_back(slot);
            }
        }
        else
        {
            bool hard = want_break && !slots.empty();
            uint16_t group = 0;
            if (token->type == TokenType::Image)
            {
                group = static_cast<uint16_t>(std::min(laid.size(), static_cast<size_t>(65535)));
            }
            else if (token->type == TokenType::Header || token->header_level > 0)
            {
                size_t need = laid.size() + 1;
                group = static_cast<uint16_t>(std::min(need, static_cast<size_t>(65535)));
            }
            for (size_t i = 0; i < laid.size(); ++i)
            {
                Slot slot;
                slot.address = laid[i];
                slot.hard_break = hard && i == 0;
                slot.group = i == 0 ? group : 0;
                slots.push_back(slot);
            }
            pending_break = false;
        }
        if (token->page_break_after)
        {
            pending_break = true;
        }
        cached_page_size = 0;
    }

    return true;
}

std::optional<DocAddr> PageIndex::progress_address() const
{
    if (slots.empty())
    {
        return std::nullopt;
    }
    return slots.back().address;
}

int PageIndex::total_lines() const
{
    return static_cast<int>(slots.size());
}

int PageIndex::line_for_address(DocAddr address, bool prefer_blank) const
{
    if (slots.empty())
    {
        return 0;
    }

    auto it_line = std::lower_bound(
        slots.begin(),
        slots.end(),
        address,
        [](const Slot &slot, DocAddr addr) { return slot.address < addr; }
    );
    if (it_line == slots.end() || it_line->address != address)
    {
        if (it_line == slots.begin())
        {
            return 0;
        }
        return static_cast<int>(it_line - slots.begin()) - 1;
    }

    int first = static_cast<int>(it_line - slots.begin());
    int match = -1;
    for (int i = first; i < static_cast<int>(slots.size()) && slots[i].address == address; ++i)
    {
        if (slots[i].blank == prefer_blank)
        {
            match = i;
            break;
        }
    }
    if (match >= 0)
    {
        return match;
    }
    return first;
}

void PageIndex::ensure_pages(int page_size) const
{
    if (!complete)
    {
        return;
    }
    if (page_size < 1)
    {
        page_size = 1;
    }
    if (cached_page_size == page_size)
    {
        return;
    }

    page_starts.clear();
    cached_page_size = page_size;
    const int n = static_cast<int>(slots.size());
    int i = 0;
    while (i < n)
    {
        // Blank separator lines between paragraphs or in front of a chapter title
        // must not open a page. If the previous page is already full they would
        // land at the top of the next one, so skip them and start on content.
        int content = i;
        while (content < n && slots[content].blank)
        {
            ++content;
        }
        if (content >= n)
        {
            break; // only blank lines are left
        }
        i = content;

        page_starts.push_back(i);
        bool page_has_content = false;
        int used = 0;
        while (i < n && used < page_size)
        {
            bool at_top = i == page_starts.back();
            const Slot &slot = slots[i];
            if (!at_top && page_has_content && slot.hard_break)
            {
                break;
            }
            if (
                slot.group > 1
                && used > 0
                && page_has_content
                && used + static_cast<int>(slot.group) > page_size
            )
            {
                break;
            }
            if (!slot.blank)
            {
                page_has_content = true;
            }
            ++i;
            ++used;
        }
    }
}

int PageIndex::page_count(int lines_per_page) const
{
    ensure_pages(lines_per_page);
    return static_cast<int>(page_starts.size());
}

int PageIndex::page_of_line(int line, int lines_per_page) const
{
    ensure_pages(lines_per_page);
    if (page_starts.empty())
    {
        return 0;
    }
    if (line < 0)
    {
        line = 0;
    }
    if (line >= static_cast<int>(slots.size()))
    {
        line = static_cast<int>(slots.size()) - 1;
    }
    auto it_page = std::upper_bound(page_starts.begin(), page_starts.end(), line);
    if (it_page == page_starts.begin())
    {
        return 0;
    }
    return static_cast<int>(it_page - page_starts.begin()) - 1;
}

int PageIndex::line_for_navigation(int line, int page_size) const
{
    ensure_pages(page_size);
    if (slots.empty())
    {
        return 0;
    }
    if (line < 0)
    {
        line = 0;
    }
    if (line >= static_cast<int>(slots.size()))
    {
        line = static_cast<int>(slots.size()) - 1;
    }
    if (!slots[line].blank)
    {
        return line;
    }

    int next = line;
    while (next < static_cast<int>(slots.size()) && slots[next].blank)
    {
        ++next;
    }
    if (next >= static_cast<int>(slots.size()))
    {
        return line;
    }

    bool starts_page = first_line_of_page(page_of_line(next, page_size), page_size) == next;
    if (slots[next].hard_break || (starts_page && slots[next].group > 1))
    {
        return next;
    }
    return line;
}

int PageIndex::first_line_of_page(int page, int lines_per_page) const
{
    ensure_pages(lines_per_page);
    if (page_starts.empty())
    {
        return 0;
    }
    if (page < 0)
    {
        page = 0;
    }
    if (page >= static_cast<int>(page_starts.size()))
    {
        page = static_cast<int>(page_starts.size()) - 1;
    }
    return page_starts[page];
}
