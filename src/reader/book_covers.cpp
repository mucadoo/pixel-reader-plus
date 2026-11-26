#include "./book_covers.h"
#include "filetypes/cbz/cbz_pages.h"

#include "filetypes/epub/epub_metadata.h"
#include "sys/screen.h"
#include "util/sdl_pointer.h"
#include "util/sdl_utils.h"
#include "util/zip_utils.h"

#include "extern/hash-library/md5.h"

#include <SDL/SDL.h>
#include <libxml/parser.h>
#include <zip.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>

namespace
{

constexpr int MAX_THUMB_SIDE = 1024;
constexpr size_t MAX_SLOTS = 100;
constexpr int MAX_DISK_LOADS_PER_CALL = 6;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
constexpr Uint32 RMASK = 0xff000000;
constexpr Uint32 GMASK = 0x00ff0000;
constexpr Uint32 BMASK = 0x0000ff00;
constexpr Uint32 AMASK = 0x000000ff;
#else
constexpr Uint32 RMASK = 0x000000ff;
constexpr Uint32 GMASK = 0x0000ff00;
constexpr Uint32 BMASK = 0x00ff0000;
constexpr Uint32 AMASK = 0xff000000;
#endif

std::string lowercase(std::string text)
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

bool is_epub(const std::filesystem::path &book)
{
    const std::string ext = lowercase(book.extension().string());
    return ext == ".epub" || ext == ".cbz";
}

std::string percent_decode(const std::string &in)
{
    std::string out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) && std::isxdigit(static_cast<unsigned char>(in[i + 2])))
        {
            out.push_back(static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        }
        else
        {
            out.push_back(in[i]);
        }
    }
    return out;
}

bool is_image(const ManifestItem &item)
{
    return lowercase(item.media_type).rfind("image/", 0) == 0;
}

std::string image_format_name(const std::string &media_type)
{
    std::string type = lowercase(media_type);
    if (type.find("png") != std::string::npos)
    {
        return "PNG";
    }
    if (type.find("gif") != std::string::npos)
    {
        return "GIF";
    }
    return "JPG";
}

// Find the cover picture in an epub and return its bytes.
bool read_epub_cover(const std::filesystem::path &book, std::vector<char> &bytes, std::string &format)
{
    int err = 0;
    zip_t *zip = zip_open(book.c_str(), ZIP_RDONLY, &err);
    if (!zip)
    {
        return false;
    }

    bool ok = false;
    try
    {
        auto container = read_zip_file_str(zip, EPUB_CONTAINER_PATH);
        std::string rootfile = container.empty() ? std::string() : epub_parse_rootfile_path(container.data());
        auto package_xml = rootfile.empty() ? std::vector<char>() : read_zip_file_str(zip, rootfile);

        PackageContents package;
        if (!package_xml.empty() && epub_parse_package_contents(rootfile, package_xml.data(), package))
        {
            const ManifestItem *chosen = nullptr;

            // EPUB 3 marks the cover in the manifest.
            for (const auto &entry : package.id_to_manifest_item)
            {
                const ManifestItem &item = entry.second;
                if (is_image(item) && item.properties.find("cover-image") != std::string::npos)
                {
                    if (!chosen || item.href < chosen->href)
                    {
                        chosen = &item;
                    }
                }
            }

            // EPUB 2 names the manifest id in a meta element.
            if (!chosen && !package.cover_id.empty())
            {
                auto found = package.id_to_manifest_item.find(package.cover_id);
                if (found != package.id_to_manifest_item.end() && is_image(found->second))
                {
                    chosen = &found->second;
                }
            }

            // Last resort: a picture with "cover" in its name.
            if (!chosen)
            {
                for (const auto &entry : package.id_to_manifest_item)
                {
                    const ManifestItem &item = entry.second;
                    if (!is_image(item))
                    {
                        continue;
                    }
                    if (lowercase(entry.first).find("cover") != std::string::npos
                        || lowercase(item.href).find("cover") != std::string::npos)
                    {
                        if (!chosen || item.href < chosen->href)
                        {
                            chosen = &item;
                        }
                    }
                }
            }

            if (chosen)
            {
                bytes = read_zip_file_str(zip, chosen->href_absolute);
                if (bytes.empty())
                {
                    bytes = read_zip_file_str(zip, percent_decode(chosen->href_absolute));
                }
                if (!bytes.empty())
                {
                    bytes.pop_back(); // read_zip_file_str adds a terminating zero
                    format = image_format_name(chosen->media_type);
                    ok = !bytes.empty();
                }
            }
        }
    }
    catch (...)
    {
        ok = false;
    }

    zip_close(zip);
    return ok;
}

// Shrink the picture to fit the box, keeping its shape. Output is packed RGB.
bool make_thumbnail(
    const std::vector<char> &bytes,
    const std::string &format,
    int box_w,
    int box_h,
    std::vector<unsigned char> &rgb,
    int &out_w,
    int &out_h
)
{
    surface_unique_ptr holder { SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1, 32, RMASK, GMASK, BMASK, AMASK) };
    if (!holder)
    {
        return false;
    }

    surface_unique_ptr image = load_surface_from_ptr(
        bytes.data(),
        static_cast<uint32_t>(bytes.size()),
        format,
        holder->format
    );
    if (!image || image->w <= 0 || image->h <= 0 || image->format->BytesPerPixel != 4)
    {
        return false;
    }

    const int src_w = image->w;
    const int src_h = image->h;
    double scale = std::min(static_cast<double>(box_w) / src_w, static_cast<double>(box_h) / src_h);
    out_w = std::clamp(static_cast<int>(src_w * scale), 1, MAX_THUMB_SIDE);
    out_h = std::clamp(static_cast<int>(src_h * scale), 1, MAX_THUMB_SIDE);
    rgb.assign(static_cast<size_t>(out_w) * out_h * 3, 0);

    if (SDL_MUSTLOCK(image.get()))
    {
        SDL_LockSurface(image.get());
    }

    const Uint8 *base = static_cast<const Uint8 *>(image->pixels);
    const int pitch = image->pitch;
    const SDL_PixelFormat *fmt = image->format;
    const int samples = 4; // per axis, spread over the source area for each output pixel

    for (int dy = 0; dy < out_h; ++dy)
    {
        const int y0 = static_cast<int>(static_cast<long long>(dy) * src_h / out_h);
        const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<long long>(dy + 1) * src_h / out_h));
        for (int dx = 0; dx < out_w; ++dx)
        {
            const int x0 = static_cast<int>(static_cast<long long>(dx) * src_w / out_w);
            const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<long long>(dx + 1) * src_w / out_w));

            unsigned r = 0;
            unsigned g = 0;
            unsigned b = 0;
            unsigned count = 0;
            for (int j = 0; j < samples; ++j)
            {
                const int sy = std::min(src_h - 1, y0 + ((y1 - y0) * (2 * j + 1)) / (2 * samples));
                const Uint8 *row = base + static_cast<size_t>(sy) * pitch;
                for (int i = 0; i < samples; ++i)
                {
                    const int sx = std::min(src_w - 1, x0 + ((x1 - x0) * (2 * i + 1)) / (2 * samples));
                    Uint32 pixel = *reinterpret_cast<const Uint32 *>(row + static_cast<size_t>(sx) * 4);
                    r += (pixel & fmt->Rmask) >> fmt->Rshift;
                    g += (pixel & fmt->Gmask) >> fmt->Gshift;
                    b += (pixel & fmt->Bmask) >> fmt->Bshift;
                    ++count;
                }
            }
            unsigned char *out = &rgb[(static_cast<size_t>(dy) * out_w + dx) * 3];
            out[0] = static_cast<unsigned char>(r / count);
            out[1] = static_cast<unsigned char>(g / count);
            out[2] = static_cast<unsigned char>(b / count);
        }
    }

    if (SDL_MUSTLOCK(image.get()))
    {
        SDL_UnlockSurface(image.get());
    }
    return true;
}

// File format: "PXC1", width and height as 16-bit little endian, then RGB bytes.
bool write_cover_file(const std::filesystem::path &path, const std::vector<unsigned char> &rgb, int w, int h)
{
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        const unsigned char header[8] = {
            'P', 'X', 'C', '1',
            static_cast<unsigned char>(w & 0xff), static_cast<unsigned char>((w >> 8) & 0xff),
            static_cast<unsigned char>(h & 0xff), static_cast<unsigned char>((h >> 8) & 0xff),
        };
        out.write(reinterpret_cast<const char *>(header), sizeof(header));
        out.write(reinterpret_cast<const char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
        if (!out)
        {
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    return !ec;
}

SDL_Surface *read_cover_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    unsigned char header[8];
    if (!in || !in.read(reinterpret_cast<char *>(header), sizeof(header)))
    {
        return nullptr;
    }
    if (header[0] != 'P' || header[1] != 'X' || header[2] != 'C' || header[3] != '1')
    {
        return nullptr;
    }
    const int w = header[4] | (header[5] << 8);
    const int h = header[6] | (header[7] << 8);
    if (w < 1 || h < 1 || w > MAX_THUMB_SIDE || h > MAX_THUMB_SIDE)
    {
        return nullptr;
    }

    std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
    if (!in.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size())))
    {
        return nullptr;
    }

    SDL_Surface *raw = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, RMASK, GMASK, BMASK, 0);
    if (!raw)
    {
        return nullptr;
    }
    for (int y = 0; y < h; ++y)
    {
        Uint8 *row = static_cast<Uint8 *>(raw->pixels) + static_cast<size_t>(y) * raw->pitch;
        for (int x = 0; x < w; ++x)
        {
            const unsigned char *px = &rgb[(static_cast<size_t>(y) * w + x) * 3];
            *reinterpret_cast<Uint32 *>(row + static_cast<size_t>(x) * 4) = SDL_MapRGB(raw->format, px[0], px[1], px[2]);
        }
    }

    SDL_PixelFormat *target = get_render_surface_format();
    if (!target)
    {
        return raw;
    }
    SDL_Surface *converted = SDL_ConvertSurface(raw, target, 0);
    SDL_FreeSurface(raw);
    return converted;
}

} // namespace

bool read_book_cover(const std::filesystem::path &book, std::vector<char> &bytes, std::string &format)
{
    std::string ext = book.extension().string();
    for (char &c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".cbz")
    {
        // The first page is the cover.
        const auto pages = cbz_page_names(book);
        if (pages.empty())
        {
            return false;
        }
        int err = 0;
        zip_t *zip = zip_open(book.c_str(), ZIP_RDONLY, &err);
        if (!zip)
        {
            return false;
        }
        bytes = read_zip_file_str(zip, pages.front());
        zip_close(zip);
        if (bytes.empty())
        {
            return false;
        }
        bytes.pop_back(); // read_zip_file_str adds a terminating zero
        format = cbz_image_format(pages.front());
        return !bytes.empty();
    }
    return ext == ".epub" && read_epub_cover(book, bytes, format);
}

BookCovers::BookCovers(std::filesystem::path dir)
    : cache_dir(std::move(dir))
{
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    xmlInitParser(); // before the worker uses libxml
    worker = std::thread([this]() { run(); });
}

BookCovers::~BookCovers()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }
    wake.notify_all();
    if (worker.joinable())
    {
        worker.join();
    }
    free_slots();
}

void BookCovers::free_slots()
{
    for (auto &entry : slots)
    {
        if (entry.second.surface)
        {
            SDL_FreeSurface(entry.second.surface);
        }
    }
    slots.clear();
}

void BookCovers::set_box(int width, int height)
{
    if (width == box_w && height == box_h)
    {
        return;
    }
    box_w = width;
    box_h = height;
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++generation;
        queue.clear();
        finished.clear();
    }
    free_slots();
}

std::string BookCovers::cache_key(const std::filesystem::path &book) const
{
    std::error_code ec;
    auto size = std::filesystem::file_size(book, ec);
    if (ec)
    {
        return "";
    }
    auto modified = std::filesystem::last_write_time(book, ec);
    if (ec)
    {
        return "";
    }
    std::string text = book.string()
        + "|" + std::to_string(static_cast<unsigned long long>(size))
        + "|" + std::to_string(static_cast<long long>(modified.time_since_epoch().count()))
        + "|" + std::to_string(box_w) + "x" + std::to_string(box_h);
    return MD5()(text);
}

void BookCovers::want(const std::vector<std::filesystem::path> &books)
{
    if (box_w <= 0 || box_h <= 0)
    {
        return;
    }

    std::unordered_set<std::string> wanted;
    std::vector<Job> fresh;
    int loads_left = MAX_DISK_LOADS_PER_CALL;

    for (const auto &book : books)
    {
        const std::string path = book.string();
        wanted.insert(path);
        if (slots.count(path))
        {
            continue;
        }

        const std::string key = cache_key(book);
        if (key.empty())
        {
            slots[path].none = true;
            continue;
        }

        const std::filesystem::path cover_file = cache_dir / (key + ".cov");
        const std::filesystem::path none_file = cache_dir / (key + ".none");
        std::error_code ec;
        if (std::filesystem::exists(cover_file, ec))
        {
            if (loads_left <= 0)
            {
                continue; // picked up on a later call
            }
            --loads_left;
            Slot slot;
            slot.surface = read_cover_file(cover_file);
            slot.none = slot.surface == nullptr;
            slots[path] = slot;
            continue;
        }
        if (std::filesystem::exists(none_file, ec) || !is_epub(book))
        {
            slots[path].none = true;
            continue;
        }

        Slot slot;
        slot.pending = true;
        slots[path] = slot;
        fresh.push_back(Job { path, key, box_w, box_h, generation });
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = queue.begin(); it != queue.end();)
        {
            if (wanted.count(it->path))
            {
                ++it;
            }
            else
            {
                slots.erase(it->path); // can be asked for again later
                it = queue.erase(it);
            }
        }
        for (auto &job : fresh)
        {
            queue.push_back(std::move(job));
        }
    }
    if (!fresh.empty())
    {
        wake.notify_one();
    }

    if (slots.size() > MAX_SLOTS)
    {
        for (auto it = slots.begin(); it != slots.end();)
        {
            if (!wanted.count(it->first) && !it->second.pending)
            {
                if (it->second.surface)
                {
                    SDL_FreeSurface(it->second.surface);
                }
                it = slots.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

SDL_Surface *BookCovers::get(const std::filesystem::path &book) const
{
    auto it = slots.find(book.string());
    return it == slots.end() ? nullptr : it->second.surface;
}

bool BookCovers::poll()
{
    std::vector<Finished> done;
    {
        std::lock_guard<std::mutex> lock(mutex);
        done.swap(finished);
    }

    bool changed = false;
    for (const auto &item : done)
    {
        if (item.generation != generation)
        {
            continue;
        }
        auto it = slots.find(item.path);
        if (it == slots.end())
        {
            continue; // no longer shown
        }

        Slot &slot = it->second;
        slot.pending = false;
        if (item.has_cover)
        {
            SDL_Surface *loaded = read_cover_file(cache_dir / (item.key + ".cov"));
            if (loaded)
            {
                if (slot.surface)
                {
                    SDL_FreeSurface(slot.surface);
                }
                slot.surface = loaded;
                changed = true;
            }
        }
        slot.none = slot.surface == nullptr;
    }
    return changed;
}

void BookCovers::run()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this]() { return stop || !queue.empty(); });
            if (stop)
            {
                return;
            }
            job = std::move(queue.front());
            queue.pop_front();
        }

        bool has_cover = build_cover(job);

        {
            std::lock_guard<std::mutex> lock(mutex);
            finished.push_back(Finished { job.path, job.key, job.generation, has_cover });
        }
    }
}

bool BookCovers::build_cover(const Job &job)
{
    try
    {
        std::vector<char> bytes;
        std::string format;
        if (read_book_cover(job.path, bytes, format))
        {
            std::vector<unsigned char> rgb;
            int w = 0;
            int h = 0;
            if (make_thumbnail(bytes, format, job.box_w, job.box_h, rgb, w, h)
                && write_cover_file(cache_dir / (job.key + ".cov"), rgb, w, h))
            {
                return true;
            }
        }
    }
    catch (...)
    {
    }

    // Remember that this book has no usable cover, so it is not tried again.
    std::ofstream marker(cache_dir / (job.key + ".none"), std::ios::binary | std::ios::trunc);
    return false;
}
