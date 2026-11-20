#ifndef CONTROLS_GUIDE_H_
#define CONTROLS_GUIDE_H_

inline const char *controls_guide_text()
{
    return
        "Up and down scroll this guide. Any other button closes it.\n"
        "\n"
        "Reading\n"
        "Scroll mode: up and down move one line. Hold to keep going.\n"
        "Scroll mode: left and right move one screen.\n"
        "Page mode: the D-pad and the shoulder buttons turn a whole page. Line scrolling is off.\n"
        "L1 and R1 match left and right. Settings can move that to L2 and R2.\n"
        "A shows or hides the progress line.\n"
        "B closes the book.\n"
        "X opens settings.\n"
        "Y selects a word.\n"
        "Select opens the chapter list.\n"
        "Start opens this guide.\n"
        "Menu quits. A Menu chord does not.\n"
        "\n"
        "Selecting\n"
        "The cursor starts on one word. Commas, periods, and quotes stay outside that word.\n"
        "D-pad moves the cursor. Hold to repeat. Left or right at the edge of the page turns the page.\n"
        "Start keeps the first word and adds the words you move through.\n"
        "A opens Look up and Highlight. Look up uses the word at the cursor. Highlight covers every selected word.\n"
        "B or Y leaves selection.\n"
        "\n"
        "Progress\n"
        "Scroll mode shows the percent. That number follows the text.\n"
        "Page mode shows the page and how many pages. Turning a page adds 1.\n"
        "A new chapter, a title page, and a picture that does not fit start at the top of a page.\n"
        "Choosing Page counts from the start, then jumps to the top of the page you were on.\n"
        "Until that count finishes, the percent stays up and the D-pad does not scroll.\n"
        "Book or Chapter chooses the whole book or the current chapter, for both modes.\n"
        "\n"
        "Settings\n"
        "Up and down change the row. Left and right change the value.\n"
        "Reading switches Scroll and Page.\n"
        "Controls, then A, opens this guide.\n"
        "B closes settings.\n";
}

#endif
