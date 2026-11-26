#ifndef KOSYNC_H_
#define KOSYNC_H_

#include <filesystem>
#include <string>

// Reading progress shared between devices through a KOReader sync server
// (kosync, such as the one that comes with many self-hosted setups). Over plain
// http only. The book is known by the MD5 of its file name, the "filename"
// method of KOReader, so the same file name on two devices is the same book.
namespace kosync
{

struct Config
{
    std::string server;     // such as http://192.168.1.20:8081
    std::string user;
    std::string key;        // the MD5 of the password, as the server expects
    bool auto_sync = false; // get the place on opening a book, send it on leaving
    std::string device_id;

    bool configured() const { return !server.empty() && !user.empty() && !key.empty(); }
};

// Where the settings are kept. Nothing is read or saved until this is set.
void set_config_path(const std::filesystem::path &path);
Config load();
void save(const Config &config);

std::string md5_hex(const std::string &text);
std::string document_id(const std::filesystem::path &book);

struct Place
{
    bool found = false;
    std::string progress;
    double percentage = 0.0; // 0 to 1
    std::string device;
};

bool pull(const Config &config, const std::string &document, Place &place, std::string &error);
bool push(const Config &config, const std::string &document, const std::string &progress, double percentage, std::string &error);
bool check_login(const Config &config, std::string &error);
bool create_account(const Config &config, std::string &error);

// The few JSON values the server sends. Exposed for the tests.
std::string json_string(const std::string &json, const std::string &key);
double json_number(const std::string &json, const std::string &key, double fallback = 0.0);
std::string json_quote(const std::string &text);

} // namespace kosync

#endif