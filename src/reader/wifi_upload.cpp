#include "./wifi_upload.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace
{

constexpr int CLIENT_TIMEOUT_SECONDS = 8;
constexpr size_t MAX_HEAD_BYTES = 16 * 1024;
constexpr unsigned long long MAX_BODY_BYTES = 400ull * 1024 * 1024;

std::string lower(std::string s)
{
    for (char &c : s)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

std::string trim(const std::string &s)
{
    size_t b = 0;
    size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
    {
        ++b;
    }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
    {
        --e;
    }
    return s.substr(b, e - b);
}

bool send_all(int fd, const std::string &data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
            {
                continue;
            }
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string html_escape(const std::string &text)
{
    std::string out;
    for (char c : text)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

void reply(int fd, int code, const char *reason, const std::string &body, const char *type = "text/html; charset=utf-8")
{
    std::string head = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type
        + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
    send_all(fd, head + body);
}

std::string page(const std::vector<std::string> &files, const std::string &message)
{
    std::string out =
        "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Pixel Reader</title><style>body{font-family:sans-serif;max-width:32em;margin:2em auto;padding:0 1em}"
        "input,button{font-size:1.1em;margin:.5em 0}button{padding:.5em 1.2em}</style></head><body>"
        "<h1>Send books</h1><p>Choose epub, cbz, fb2, md, txt or html files. They are saved in the Downloads folder of the library.</p>"
        "<form method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"books\" multiple accept=\".epub,.cbz,.fb2,.md,.markdown,.txt,.html,.htm\"><br>"
        "<button type=\"submit\">Send</button></form>";
    if (!message.empty())
    {
        out += "<p><b>" + html_escape(message) + "</b></p>";
    }
    if (!files.empty())
    {
        out += "<h2>Received</h2><ul>";
        for (const std::string &file : files)
        {
            out += "<li>" + html_escape(file) + "</li>";
        }
        out += "</ul>";
    }
    out += "</body></html>";
    return out;
}

// The value of name="..." in a header such as Content-Disposition.
std::string header_param(const std::string &header, const std::string &name)
{
    const std::string key = name + "=\"";
    size_t at = 0;
    while ((at = header.find(key, at)) != std::string::npos)
    {
        // It must be a whole word: "filename=" is not "name="
        if (at == 0 || header[at - 1] == ' ' || header[at - 1] == ';')
        {
            const size_t begin = at + key.size();
            const size_t end = header.find('"', begin);
            return end == std::string::npos ? std::string() : header.substr(begin, end - begin);
        }
        at += key.size();
    }
    return std::string();
}

std::string unique_path_name(const std::filesystem::path &folder, const std::string &name)
{
    std::error_code ec;
    if (!std::filesystem::exists(folder / name, ec))
    {
        return name;
    }
    const std::filesystem::path p(name);
    const std::string stem = p.stem().string();
    const std::string ext = p.extension().string();
    for (int n = 2; n < 1000; ++n)
    {
        const std::string candidate = stem + " (" + std::to_string(n) + ")" + ext;
        if (!std::filesystem::exists(folder / candidate, ec))
        {
            return candidate;
        }
    }
    return name;
}

} // namespace

std::string upload_safe_name(const std::string &raw)
{
    std::string name = raw;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        name = name.substr(slash + 1);
    }
    std::string clean;
    for (unsigned char c : name)
    {
        if (c < 32 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            clean += '_';
        }
        else
        {
            clean += static_cast<char>(c);
        }
    }
    clean = trim(clean);
    while (!clean.empty() && clean.front() == '.')
    {
        clean.erase(0, 1);
    }
    const size_t dot = clean.rfind('.');
    if (dot == std::string::npos || dot == 0)
    {
        return std::string();
    }
    const std::string ext = lower(clean.substr(dot));
    static const char *allowed[] = {".epub", ".cbz", ".fb2", ".md", ".markdown", ".txt", ".html", ".htm"};
    bool ok = false;
    for (const char *a : allowed)
    {
        ok = ok || ext == a;
    }
    if (!ok)
    {
        return std::string();
    }
    constexpr size_t MAX_NAME = 120;
    if (clean.size() > MAX_NAME)
    {
        const std::string stem = clean.substr(0, dot);
        size_t cut = MAX_NAME - ext.size();
        while (cut > 0 && (static_cast<unsigned char>(stem[cut]) & 0xC0) == 0x80)
        {
            --cut;
        }
        clean = stem.substr(0, cut) + clean.substr(dot);
    }
    return clean;
}

std::string local_ip_address()
{
    ifaddrs *list = nullptr;
    if (::getifaddrs(&list) != 0)
    {
        return std::string();
    }
    std::string wlan;
    std::string other;
    for (ifaddrs *a = list; a; a = a->ifa_next)
    {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK))
        {
            continue;
        }
        char text[INET_ADDRSTRLEN];
        const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(a->ifa_addr);
        if (!::inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)))
        {
            continue;
        }
        if (std::strncmp(a->ifa_name, "wlan", 4) == 0 && wlan.empty())
        {
            wlan = text;
        }
        else if (other.empty())
        {
            other = text;
        }
    }
    ::freeifaddrs(list);
    return wlan.empty() ? other : wlan;
}

UploadServer::UploadServer(std::filesystem::path folder)
    : folder(std::move(folder))
{
}

UploadServer::~UploadServer()
{
    stop();
}

bool UploadServer::start(int first_port, std::string &error)
{
    stop();
    for (int candidate = first_port; candidate < first_port + 10; ++candidate)
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            error = "Could not open a network socket";
            return false;
        }
        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(static_cast<uint16_t>(candidate));
        if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 && ::listen(fd, 4) == 0)
        {
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            listener = fd;
            port = candidate;
            return true;
        }
        ::close(fd);
    }
    error = "Could not listen on a port";
    return false;
}

void UploadServer::stop()
{
    if (listener >= 0)
    {
        ::close(listener);
        listener = -1;
    }
}

bool UploadServer::poll()
{
    if (listener < 0)
    {
        return false;
    }
    const int client = ::accept(listener, nullptr, nullptr);
    if (client < 0)
    {
        return false;
    }
    ::fcntl(client, F_SETFL, ::fcntl(client, F_GETFL, 0) & ~O_NONBLOCK);
    timeval tv;
    tv.tv_sec = CLIENT_TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    const size_t before = received.size();
    const std::string before_message = last_message;
    handle(client);
    ::close(client);
    return received.size() != before || last_message != before_message;
}

void UploadServer::handle(int client)
{
    // The request line and headers
    std::string head;
    std::string rest;
    char buf[16384];
    for (;;)
    {
        const ssize_t n = ::recv(client, buf, sizeof(buf), 0);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            return;
        }
        head.append(buf, static_cast<size_t>(n));
        const size_t end = head.find("\r\n\r\n");
        if (end != std::string::npos)
        {
            rest = head.substr(end + 4);
            head.resize(end);
            break;
        }
        if (head.size() > MAX_HEAD_BYTES)
        {
            reply(client, 431, "Request Header Fields Too Large", "Too large");
            return;
        }
    }
    const size_t line_end = head.find("\r\n");
    const std::string request_line = head.substr(0, line_end);
    const size_t sp1 = request_line.find(' ');
    const size_t sp2 = request_line.find(' ', sp1 == std::string::npos ? 0 : sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos)
    {
        reply(client, 400, "Bad Request", "Bad request");
        return;
    }
    const std::string method = request_line.substr(0, sp1);
    std::string path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
    const size_t query = path.find('?');
    if (query != std::string::npos)
    {
        path.resize(query);
    }

    unsigned long long length = 0;
    std::string content_type;
    for (size_t pos = line_end; pos != std::string::npos && pos < head.size();)
    {
        const size_t next = head.find("\r\n", pos + 2);
        const std::string line = head.substr(pos + 2, next == std::string::npos ? std::string::npos : next - pos - 2);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            const std::string name = lower(line.substr(0, colon));
            const std::string value = trim(line.substr(colon + 1));
            if (name == "content-length")
            {
                length = std::strtoull(value.c_str(), nullptr, 10);
            }
            else if (name == "content-type")
            {
                content_type = value;
            }
        }
        pos = next;
    }

    if (method == "GET" && path == "/")
    {
        reply(client, 200, "OK", page(received, last_message));
        return;
    }
    if (!(method == "POST" && path == "/upload"))
    {
        reply(client, 404, "Not Found", "Not found", "text/plain");
        return;
    }
    const size_t b = lower(content_type).find("boundary=");
    if (lower(content_type).find("multipart/form-data") == std::string::npos || b == std::string::npos)
    {
        reply(client, 400, "Bad Request", "Send the files with the form on the page", "text/plain");
        return;
    }
    std::string boundary = content_type.substr(b + 9);
    if (!boundary.empty() && boundary[0] == '"')
    {
        const size_t close = boundary.find('"', 1);
        boundary = boundary.substr(1, close == std::string::npos ? std::string::npos : close - 1);
    }
    else
    {
        const size_t semi = boundary.find(';');
        if (semi != std::string::npos)
        {
            boundary.resize(semi);
        }
    }
    if (boundary.empty() || boundary.size() > 200 || length == 0 || length > MAX_BODY_BYTES)
    {
        reply(client, 400, "Bad Request", "The upload is empty or too large", "text/plain");
        return;
    }

    // The body, one piece at a time. A leading CRLF makes the first boundary like the others.
    const std::string delimiter = "\r\n--" + boundary;
    enum class State { Boundary, Headers, Data, Done };
    State state = State::Boundary;
    std::string data = "\r\n" + rest;
    unsigned long long remaining = length > rest.size() ? length - rest.size() : 0;
    std::ofstream out;
    std::filesystem::path part_file;
    std::filesystem::path final_file;
    std::string saved_name;
    int saved_count = 0;
    int skipped_count = 0;
    bool failed = false;
    std::string failure;
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);

    auto close_part = [&](bool keep) {
        if (out.is_open())
        {
            out.close();
        }
        if (!part_file.empty())
        {
            if (keep)
            {
                std::error_code rename_ec;
                std::filesystem::rename(part_file, final_file, rename_ec);
                if (rename_ec)
                {
                    failed = true;
                    failure = "Could not save " + saved_name;
                    std::filesystem::remove(part_file, rename_ec);
                }
                else
                {
                    received.push_back(saved_name);
                    ++saved_count;
                }
            }
            else
            {
                std::error_code rm_ec;
                std::filesystem::remove(part_file, rm_ec);
            }
            part_file.clear();
        }
    };

    for (;;)
    {
        bool progressed = true;
        while (progressed && state != State::Done && !failed)
        {
            progressed = false;
            if (state == State::Boundary)
            {
                const size_t at = data.find(delimiter);
                if (at == std::string::npos)
                {
                    if (data.size() > delimiter.size())
                    {
                        data.erase(0, data.size() - delimiter.size());
                    }
                    break;
                }
                data.erase(0, at + delimiter.size());
                if (data.size() < 2)
                {
                    data.insert(0, delimiter); // wait for more, then look again
                    break;
                }
                if (data.compare(0, 2, "--") == 0)
                {
                    state = State::Done;
                    break;
                }
                const size_t eol = data.find("\r\n");
                if (eol == std::string::npos)
                {
                    data.insert(0, delimiter);
                    break;
                }
                data.erase(0, eol + 2);
                state = State::Headers;
                progressed = true;
            }
            else if (state == State::Headers)
            {
                const size_t end = data.find("\r\n\r\n");
                if (end == std::string::npos)
                {
                    if (data.size() > MAX_HEAD_BYTES)
                    {
                        failed = true;
                        failure = "The upload is not in the expected form";
                    }
                    break;
                }
                const std::string part_head = data.substr(0, end);
                data.erase(0, end + 4);
                std::string filename;
                for (size_t pos = 0; pos <= part_head.size();)
                {
                    size_t next = part_head.find("\r\n", pos);
                    if (next == std::string::npos)
                    {
                        next = part_head.size();
                    }
                    const std::string line = part_head.substr(pos, next - pos);
                    if (lower(line).compare(0, 20, "content-disposition:") == 0)
                    {
                        filename = header_param(line, "filename");
                    }
                    pos = next + 2;
                }
                const std::string safe = upload_safe_name(filename);
                if (!safe.empty())
                {
                    saved_name = unique_path_name(folder, safe);
                    final_file = folder / saved_name;
                    part_file = final_file;
                    part_file += ".part";
                    out.open(part_file, std::ios::binary | std::ios::trunc);
                    if (!out)
                    {
                        failed = true;
                        failure = "Could not write to the card";
                        part_file.clear();
                    }
                }
                else if (!filename.empty())
                {
                    ++skipped_count;
                }
                state = State::Data;
                progressed = true;
            }
            else if (state == State::Data)
            {
                const size_t at = data.find(delimiter);
                const size_t usable = at != std::string::npos ? at
                    : (data.size() > delimiter.size() ? data.size() - delimiter.size() : 0);
                if (out.is_open() && usable > 0)
                {
                    out.write(data.data(), static_cast<std::streamsize>(usable));
                    if (!out)
                    {
                        failed = true;
                        failure = "The card is full or not writable";
                    }
                }
                data.erase(0, usable);
                if (at != std::string::npos)
                {
                    close_part(!failed);
                    state = State::Boundary;
                    progressed = true;
                }
                else
                {
                    break;
                }
            }
        }
        if (state == State::Done || failed)
        {
            break;
        }
        if (remaining == 0)
        {
            failed = true;
            failure = "The upload was cut short";
            break;
        }
        const ssize_t n = ::recv(client, buf, static_cast<size_t>(std::min<unsigned long long>(sizeof(buf), remaining)), 0);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            failed = true;
            failure = "The connection was lost during the upload";
            break;
        }
        data.append(buf, static_cast<size_t>(n));
        remaining -= static_cast<unsigned long long>(n);
    }
    if (out.is_open() || !part_file.empty())
    {
        out.close();
        close_part(false); // a file that was not finished is not kept
    }

    std::string message;
    if (failed)
    {
        message = failure;
    }
    else
    {
        message = "Saved " + std::to_string(saved_count) + (saved_count == 1 ? " file" : " files");
        if (skipped_count > 0)
        {
            message += ", skipped " + std::to_string(skipped_count) + " of a type the reader does not open";
        }
    }
    last_message = message;
    reply(client, failed ? 500 : 200, failed ? "Server Error" : "OK", page(received, message));
}
