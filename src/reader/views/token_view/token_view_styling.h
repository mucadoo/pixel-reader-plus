#ifndef TOKEN_VIEW_STYLING_H_
#define TOKEN_VIEW_STYLING_H_

#include "reader/progress_reporting.h"
#include "reader/reading_mode.h"

#include <functional>
#include <memory>
#include <string>

struct TokenViewStylingState;

class TokenViewStyling
{
    std::unique_ptr<TokenViewStylingState> state;
    void notify_subscribers() const;

public:
    TokenViewStyling(bool show_title_bar, ProgressReporting progress_reporting, ReadingMode reading_mode);
    virtual ~TokenViewStyling();

    // Title bar
    bool get_show_title_bar() const;
    void set_show_title_bar(bool show_title_bar);

    // Progress reporting
    ProgressReporting get_progress_reporting() const;
    void set_progress_reporting(ProgressReporting progress_reporting);

    ReadingMode get_reading_mode() const;
    void set_reading_mode(ReadingMode reading_mode);

    // Subscribe to any changes
    uint32_t subscribe_to_changes(std::function<void()> callback);
    void unsubscribe_from_changes(uint32_t sub_id);
};

#endif
