#ifndef EPUB_FONTS_H_
#define EPUB_FONTS_H_

#include <string>
#include <vector>

// A font file an epub's stylesheet declares with @font-face.
struct FontFace
{
    std::string family; // lowercase
    bool bold = false;
    bool italic = false;
    std::string src; // path inside the epub
};

// The @font-face rules of one stylesheet. css_dir is where the stylesheet sits inside the epub.
// Only truetype and opentype files are kept; the reader cannot draw woff.
std::vector<FontFace> parse_font_faces(const std::string &css, const std::string &css_dir);

// The first family named by the body, html or p rule of a stylesheet, lowercase. Empty when none.
std::string parse_body_family(const std::string &css);

#endif