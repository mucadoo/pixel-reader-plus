#ifndef BOOK_COVERS_H_
#define BOOK_COVERS_H_

#include <SDL/SDL_video.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Small cover pictures for the book grid.
//
// Pulling a cover out of an epub and shrinking it can take a good fraction of a
// second on the handheld, so that work happens on a background thread and the
// finished thumbnail is saved in a cache folder. After that a thumbnail loads in
// a moment, and the app never waits on a cover.
class BookCovers
{
    struct Job
    {
        std::string path;
        std::string key;
        int box_w = 0;
        int box_h = 0;
        uint32_t generation = 0;
    };

    struct Finished
    {
        std::string path;
        std::string key;
        uint32_t generation = 0;
        bool has_cover = false;
    };

    struct Slot
    {
        SDL_Surface *surface = nullptr;
        bool pending = false;
        bool none = false;
    };

    std::filesystem::path cache_dir;
    int box_w = 0;
    int box_h = 0;
    uint32_t generation = 0;
    std::unordered_map<std::string, Slot> slots; // main thread only

    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Job> queue;
    std::vector<Finished> finished;
    bool stop = false;
    std::thread worker;

    void run();
    bool build_cover(const Job &job);
    std::string cache_key(const std::filesystem::path &book) const;
    void free_slots();

public:
    explicit BookCovers(std::filesystem::path cache_dir);
    ~BookCovers();
    BookCovers(const BookCovers &) = delete;
    BookCovers &operator=(const BookCovers &) = delete;

    // The box each thumbnail has to fit in. A change drops the pictures loaded so far.
    void set_box(int width, int height);

    // The books on screen, nearest first. Starts making the ones that are missing
    // and drops queued work for books that are no longer shown.
    void want(const std::vector<std::filesystem::path> &books);

    // The thumbnail, or null while it is being made or when the book has no cover.
    SDL_Surface *get(const std::filesystem::path &book) const;

    // Pick up thumbnails the worker finished. True when there is something new to draw.
    bool poll();
};

#endif
