#ifndef STORAGE_USAGE_H_
#define STORAGE_USAGE_H_

#include <cstdint>
#include <filesystem>
#include <string>

// How much room the files of the reader take, and ways to give some back.
namespace storage_usage
{

// The bytes of every file under the folder, 0 when there is none.
std::uintmax_t folder_size(const std::filesystem::path &folder);
// The bytes of the files directly in the folder whose names end in the suffix; empty suffix means all.
std::uintmax_t files_size(const std::filesystem::path &folder, const std::string &suffix, bool want_suffix);

// Deletes what is in the folder and keeps the folder. Returns the bytes freed.
std::uintmax_t clear_folder(const std::filesystem::path &folder);
// Deletes the files directly in the folder with (or, when want_suffix is false, without) the suffix.
std::uintmax_t clear_files(const std::filesystem::path &folder, const std::string &suffix, bool want_suffix);

// "12.3 MB", "512 KB", "0 B".
std::string format_bytes(std::uintmax_t bytes);

// Free space on the card holding the path, -1 when unknown.
long long free_bytes(const std::filesystem::path &path);

} // namespace storage_usage

#endif
