#include "./kosync.h"

#include "./opds.h"

#include "extern/hash-library/md5.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <random>

namespace kosync
{

namespace
{

std::filesystem::path g_path;
constexpr int SYNC_TIMEOUT_SECONDS = 6;

// Short waits while the reader is in the foreground, put back on the way out.
struct ShortTimeout
{
    ShortTimeout() { opds::set_timeout(SYNC_TIMEOUT_SECONDS); }
    ~ShortTimeout() { opds::set_timeout(20); }
};

std::string base_url(const Config &config)
{
    std::string url = config.server;
    while (!url.empty() && (url.back() == '/' || url.back() == ' '))
    {
        url.pop_back();
    }
    return url;
}

std::vector<std::pair<std::string, std::string>> headers_for(const Config &config, bool json_body)
{
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Accept", "application/vnd.koreader.v1+json"},
        {"x-auth-user", config.user},
        {"x-auth-key", config.key},
    };
    if (json_body)
    {
        headers.push_back({"Content-Type", "application/json"});
    }
    return headers;
}

std::string server_message(const opds::Response &response)
{
    const std::string message = json_string(response.body, "message");
    return message.empty() ? "The server answered " + std::to_string(response.status) : message;
}

std::string random_hex(size_t length)
{
    std::random_device device;
    std::mt19937_64 engine((static_cast<unsigned long long>(device()) << 32) ^ static_cast<unsigned long long>(std::time(nullptr)));
    static const char *digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < length; ++i)
    {
        out += digits[engine() & 15];
    }
    return out;
}

} // namespace

void set_config_path(const std::filesystem::path &path)
{
    g_path = path;
}

Config load()
{
    Config config;
    if (g_path.empty())
    {
        return config;
    }
    std::ifstream in(g_path);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "server") config.server = value;
        else if (key == "user") config.user = value;
        else if (key == "key") config.key = value;
        else if (key == "auto") config.auto_sync = value == "1";
        else if (key == "device_id") config.device_id = value;
    }
    if (config.device_id.empty())
    {
        config.device_id = random_hex(32);
    }
    return config;
}

void save(const Config &config)
{
    if (g_path.empty())
    {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(g_path.parent_path(), ec);
    std::ofstream out(g_path, std::ios::trunc);
    out << "server=" << config.server << "\n"
        << "user=" << config.user << "\n"
        << "key=" << config.key << "\n"
        << "auto=" << (config.auto_sync ? 1 : 0) << "\n"
        << "device_id=" << (config.device_id.empty() ? random_hex(32) : config.device_id) << "\n";
}

std::string md5_hex(const std::string &text)
{
    return MD5()(text);
}

std::string document_id(const std::filesystem::path &book)
{
    return md5_hex(book.filename().string());
}

std::string json_quote(const std::string &text)
{
    std::string out = "\"";
    for (unsigned char c : text)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32)
                {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", c);
                    out += escape;
                }
                else
                {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

namespace
{

// Where the value of "key" starts, after the colon, or npos.
size_t value_start(const std::string &json, const std::string &key)
{
    const std::string needle = "\"" + key + "\"";
    size_t at = 0;
    while ((at = json.find(needle, at)) != std::string::npos)
    {
        size_t p = at + needle.size();
        while (p < json.size() && (json[p] == ' ' || json[p] == '\n' || json[p] == '\t' || json[p] == '\r'))
        {
            ++p;
        }
        if (p < json.size() && json[p] == ':')
        {
            ++p;
            while (p < json.size() && (json[p] == ' ' || json[p] == '\n' || json[p] == '\t' || json[p] == '\r'))
            {
                ++p;
            }
            return p;
        }
        at += needle.size();
    }
    return std::string::npos;
}

} // namespace

std::string json_string(const std::string &json, const std::string &key)
{
    size_t p = value_start(json, key);
    if (p == std::string::npos || p >= json.size() || json[p] != '"')
    {
        return std::string();
    }
    ++p;
    std::string out;
    while (p < json.size() && json[p] != '"')
    {
        if (json[p] == '\\' && p + 1 < json.size())
        {
            ++p;
            switch (json[p])
            {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'u':
                {
                    if (p + 4 < json.size())
                    {
                        const unsigned long code = std::strtoul(json.substr(p + 1, 4).c_str(), nullptr, 16);
                        // Encode as UTF-8
                        if (code < 0x80)
                        {
                            out += static_cast<char>(code);
                        }
                        else if (code < 0x800)
                        {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        else
                        {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        p += 4;
                    }
                    break;
                }
                default: out += json[p];
            }
        }
        else
        {
            out += json[p];
        }
        ++p;
    }
    return out;
}

double json_number(const std::string &json, const std::string &key, double fallback)
{
    const size_t p = value_start(json, key);
    if (p == std::string::npos || p >= json.size())
    {
        return fallback;
    }
    // The number may be quoted by some servers
    const size_t from = json[p] == '"' ? p + 1 : p;
    char *end = nullptr;
    const double value = std::strtod(json.c_str() + from, &end);
    return end == json.c_str() + from ? fallback : value;
}

bool pull(const Config &config, const std::string &document, Place &place, std::string &error)
{
    place = Place();
    ShortTimeout timeout;
    opds::Response response;
    if (!opds::call("GET", base_url(config) + "/syncs/progress/" + document, headers_for(config, false), "", response, error))
    {
        return false;
    }
    if (response.status == 401)
    {
        error = "The server did not accept the user name or password";
        return false;
    }
    if (response.status == 404)
    {
        return true; // nothing saved for this book yet
    }
    if (response.status != 200)
    {
        error = server_message(response);
        return false;
    }
    place.progress = json_string(response.body, "progress");
    place.percentage = json_number(response.body, "percentage", -1.0);
    place.device = json_string(response.body, "device");
    place.found = place.percentage >= 0.0 || !place.progress.empty();
    if (place.percentage < 0.0)
    {
        place.percentage = 0.0;
    }
    return true;
}

bool push(const Config &config, const std::string &document, const std::string &progress, double percentage, std::string &error)
{
    ShortTimeout timeout;
    char number[32];
    std::snprintf(number, sizeof(number), "%.4f", percentage < 0 ? 0.0 : (percentage > 1 ? 1.0 : percentage));
    const std::string body = "{\"document\":" + json_quote(document)
        + ",\"progress\":" + json_quote(progress)
        + ",\"percentage\":" + number
        + ",\"device\":" + json_quote("Pixel Reader")
        + ",\"device_id\":" + json_quote(config.device_id) + "}";
    opds::Response response;
    if (!opds::call("PUT", base_url(config) + "/syncs/progress", headers_for(config, true), body, response, error))
    {
        return false;
    }
    if (response.status == 401)
    {
        error = "The server did not accept the user name or password";
        return false;
    }
    if (response.status != 200 && response.status != 201)
    {
        error = server_message(response);
        return false;
    }
    return true;
}

bool check_login(const Config &config, std::string &error)
{
    ShortTimeout timeout;
    opds::Response response;
    if (!opds::call("GET", base_url(config) + "/users/auth", headers_for(config, false), "", response, error))
    {
        return false;
    }
    if (response.status == 200)
    {
        return true;
    }
    error = response.status == 401 ? "The server did not accept the user name or password" : server_message(response);
    return false;
}

bool create_account(const Config &config, std::string &error)
{
    ShortTimeout timeout;
    opds::Response response;
    const std::string body = "{\"username\":" + json_quote(config.user) + ",\"password\":" + json_quote(config.key) + "}";
    if (!opds::call("POST", base_url(config) + "/users/create", headers_for(config, true), body, response, error))
    {
        return false;
    }
    if (response.status == 200 || response.status == 201)
    {
        return true;
    }
    error = server_message(response);
    return false;
}

} // namespace kosync