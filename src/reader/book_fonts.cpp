#include "reader/book_fonts.h"

#include "doc_api/doc_reader.h"

#include <SDL/SDL_ttf.h>

#include <fstream>
#include <vector>

namespace book_fonts
{

namespace
{

std::filesystem::path font_dir;
// Indexed by bold (1) and italic (2). Empty when the face is not there.
std::string files[4];
std::string layout_key;

} // namespace

void set_dir(const std::filesystem::path &dir)
{
    font_dir = dir;
}

void clear()
{
    for (std::string &file : files)
    {
        file.clear();
    }
    layout_key.clear();
}

void open(const DocReader &reader, const std::string &book_id, bool use)
{
    clear();
    if (!use || font_dir.empty())
    {
        return;
    }
    const auto faces = reader.embedded_fonts();
    if (faces.empty())
    {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(font_dir, ec);
    for (const auto &face : faces)
    {
        const int index = (face.bold ? 1 : 0) | (face.italic ? 2 : 0);
        if (!files[index].empty())
        {
            continue;
        }
        const std::string ext = std::filesystem::path(face.path).extension().string();
        const std::filesystem::path target = font_dir / (book_id + "-" + std::to_string(index) + ext);
        if (!std::filesystem::exists(target, ec))
        {
            const auto data = reader.load_resource(face.path);
            if (data.size() < 64)
            {
                continue;
            }
            std::ofstream out(target, std::ios::binary);
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            out.close();
            if (!out)
            {
                std::filesystem::remove(target, ec);
                continue;
            }
        }
        files[index] = target.string();
    }
    if (files[0].empty())
    {
        clear(); // the text face is needed, the rest is only variants
        return;
    }
    layout_key = ",bf:" + book_id;
}

bool active()
{
    return !files[0].empty();
}

std::string pick(int &style)
{
    const bool bold = (style & TTF_STYLE_BOLD) != 0;
    const bool italic = (style & TTF_STYLE_ITALIC) != 0;
    if (bold && italic && !files[3].empty())
    {
        style &= ~(TTF_STYLE_BOLD | TTF_STYLE_ITALIC);
        return files[3];
    }
    if (bold && !files[1].empty())
    {
        style &= ~TTF_STYLE_BOLD;
        return files[1];
    }
    if (italic && !files[2].empty())
    {
        style &= ~TTF_STYLE_ITALIC;
        return files[2];
    }
    return files[0];
}

std::string key()
{
    return layout_key;
}

} // namespace book_fonts