#include "./opds.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace opds
{

namespace
{

int g_timeout_seconds = 20;
constexpr int MAX_REDIRECTS = 6;
constexpr size_t MAX_HEADER_BYTES = 64 * 1024;
constexpr size_t MAX_PAGE_BYTES = 8 * 1024 * 1024;

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

bool starts_with(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
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

std::string base64(const std::string &in)
{
    static const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i + 2 < in.size())
    {
        const unsigned v = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8) | static_cast<unsigned char>(in[i + 2]);
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += table[(v >> 6) & 63];
        out += table[v & 63];
        i += 3;
    }
    if (i + 1 == in.size())
    {
        const unsigned v = static_cast<unsigned char>(in[i]) << 16;
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += "==";
    }
    else if (i + 2 == in.size())
    {
        const unsigned v = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8);
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += table[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

// Spaces and bytes above ASCII must not appear raw in a request line.
std::string encode_path(const std::string &path)
{
    static const char *digits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path)
    {
        if (c <= ' ' || c >= 127)
        {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        else
        {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// Closes the socket when it goes out of scope.
struct Socket
{
    int fd = -1;
    ~Socket()
    {
        if (fd >= 0)
        {
            ::close(fd);
        }
    }
};

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

bool connect_to(const Url &url, Socket &sock, std::string &error)
{
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *found = nullptr;
    const int rc = ::getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &found);
    if (rc != 0 || !found)
    {
        error = "Could not find " + url.host + ". Is wifi on?";
        return false;
    }
    std::unique_ptr<addrinfo, void (*)(addrinfo *)> guard(found, ::freeaddrinfo);
    for (addrinfo *a = found; a; a = a->ai_next)
    {
        const int fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0)
        {
            continue;
        }
        timeval tv;
        tv.tv_sec = g_timeout_seconds;
        tv.tv_usec = 0;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (::connect(fd, a->ai_addr, a->ai_addrlen) == 0)
        {
            sock.fd = fd;
            return true;
        }
        ::close(fd);
    }
    error = "Could not connect to " + url.host + ":" + url.port;
    return false;
}

using Sink = std::function<bool(const char *, size_t)>;

struct RequestOptions
{
    std::string method = "GET";
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int *status = nullptr; // when set, any status is accepted and reported here
};

// One request with redirects. The sink gets the body of the final answer.
bool request(const std::string &start, const Sink &sink, const Progress &progress, std::string &error, const RequestOptions &options = RequestOptions())
{
    std::string current = start;
    for (int hop = 0; hop <= MAX_REDIRECTS; ++hop)
    {
        const Url url = parse_url(current);
        if (!url.ok)
        {
            error = url.error;
            return false;
        }
        Socket sock;
        if (!connect_to(url, sock, error))
        {
            return false;
        }
        std::string host_header = url.host;
        if (url.host.find(':') != std::string::npos)
        {
            host_header = "[" + url.host + "]";
        }
        if (url.port != "80")
        {
            host_header += ":" + url.port;
        }
        std::string req = options.method + " " + encode_path(url.path) + " HTTP/1.1\r\nHost: " + host_header
            + "\r\nUser-Agent: PixelReader\r\nConnection: close\r\n";
        bool has_accept = false;
        for (const auto &header : options.headers)
        {
            has_accept = has_accept || lower(header.first) == "accept";
        }
        if (!has_accept)
        {
            req += "Accept: application/atom+xml, application/xml, */*\r\n";
        }
        if (!url.credentials.empty())
        {
            req += "Authorization: Basic " + base64(url.credentials) + "\r\n";
        }
        for (const auto &header : options.headers)
        {
            req += header.first + ": " + header.second + "\r\n";
        }
        if (!options.body.empty() || options.method == "POST" || options.method == "PUT")
        {
            req += "Content-Length: " + std::to_string(options.body.size()) + "\r\n";
        }
        req += "\r\n";
        req += options.body;
        if (!send_all(sock.fd, req))
        {
            error = "The connection was lost";
            return false;
        }

        // The header
        std::string head;
        std::string rest; // body bytes that came with the header
        char buf[16384];
        for (;;)
        {
            const ssize_t n = ::recv(sock.fd, buf, sizeof(buf), 0);
            if (n < 0 && errno == EINTR)
            {
                continue;
            }
            if (n < 0)
            {
                error = (errno == EAGAIN || errno == EWOULDBLOCK) ? "The server did not answer in time" : std::string("Network error: ") + std::strerror(errno);
                return false;
            }
            if (n == 0)
            {
                error = "The server closed the connection";
                return false;
            }
            head.append(buf, static_cast<size_t>(n));
            const size_t end = head.find("\r\n\r\n");
            if (end != std::string::npos)
            {
                rest = head.substr(end + 4);
                head.resize(end);
                break;
            }
            if (head.size() > MAX_HEADER_BYTES)
            {
                error = "The answer is not HTTP";
                return false;
            }
        }
        if (!starts_with(head, "HTTP/"))
        {
            error = "The answer is not HTTP";
            return false;
        }
        const size_t space = head.find(' ');
        const int code = space == std::string::npos ? 0 : std::atoi(head.c_str() + space + 1);
        std::string location;
        bool chunked = false;
        size_t length = 0;
        bool has_length = false;
        size_t pos = head.find("\r\n");
        while (pos != std::string::npos)
        {
            const size_t next = head.find("\r\n", pos + 2);
            const std::string line = head.substr(pos + 2, next == std::string::npos ? std::string::npos : next - pos - 2);
            const size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                const std::string name = lower(line.substr(0, colon));
                const std::string value = trim(line.substr(colon + 1));
                if (name == "location")
                {
                    location = value;
                }
                else if (name == "transfer-encoding" && lower(value).find("chunked") != std::string::npos)
                {
                    chunked = true;
                }
                else if (name == "content-length")
                {
                    length = static_cast<size_t>(std::strtoull(value.c_str(), nullptr, 10));
                    has_length = true;
                }
            }
            pos = next;
        }

        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
        {
            if (location.empty())
            {
                error = "The server sent a redirect with no place to go";
                return false;
            }
            current = resolve(current, location);
            continue;
        }
        if (options.status)
        {
            *options.status = code;
        }
        else if (code == 401 || code == 403)
        {
            error = "The server wants a login. Put it in the address as http://name:password@host";
            return false;
        }
        if (!options.status && code != 200)
        {
            error = "The server answered " + std::to_string(code);
            return false;
        }

        // The body
        ChunkDecoder decoder;
        size_t received = 0;
        const size_t total = (!chunked && has_length) ? length : 0;
        auto take = [&](const char *data, size_t len) -> bool {
            if (len == 0)
            {
                return true;
            }
            if (chunked)
            {
                std::string out;
                if (!decoder.feed(data, len, out))
                {
                    error = "The server sent a broken answer";
                    return false;
                }
                if (!out.empty())
                {
                    received += out.size();
                    if (!sink(out.data(), out.size()))
                    {
                        error = "Could not save the data";
                        return false;
                    }
                }
            }
            else
            {
                size_t usable = len;
                if (has_length && received + usable > length)
                {
                    usable = length - received;
                }
                received += usable;
                if (usable > 0 && !sink(data, usable))
                {
                    error = "Could not save the data";
                    return false;
                }
            }
            if (progress)
            {
                progress(received, total);
            }
            return true;
        };
        if (!take(rest.data(), rest.size()))
        {
            return false;
        }
        for (;;)
        {
            if (chunked ? decoder.finished() : (has_length && received >= length))
            {
                return true;
            }
            const ssize_t n = ::recv(sock.fd, buf, sizeof(buf), 0);
            if (n < 0 && errno == EINTR)
            {
                continue;
            }
            if (n < 0)
            {
                error = (errno == EAGAIN || errno == EWOULDBLOCK) ? "The transfer stopped (timed out)" : std::string("Network error: ") + std::strerror(errno);
                return false;
            }
            if (n == 0)
            {
                if ((has_length && received < length) || (chunked && !decoder.finished()))
                {
                    error = "The connection was lost before the end";
                    return false;
                }
                return true;
            }
            if (!take(buf, static_cast<size_t>(n)))
            {
                return false;
            }
        }
    }
    error = "Too many redirects";
    return false;
}

std::string text_of(xmlNode *node)
{
    std::string text;
    for (xmlNode *c = node ? node->children : nullptr; c; c = c->next)
    {
        if ((c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) && c->content)
        {
            text += reinterpret_cast<const char *>(c->content);
        }
    }
    return trim(text);
}

std::string attr_of(xmlNode *node, const char *name)
{
    xmlChar *value = xmlGetProp(node, reinterpret_cast<const xmlChar *>(name));
    if (!value)
    {
        return std::string();
    }
    std::string out(reinterpret_cast<const char *>(value));
    xmlFree(value);
    return out;
}

bool is_named(xmlNode *node, const char *name)
{
    return node->type == XML_ELEMENT_NODE && node->name && std::strcmp(reinterpret_cast<const char *>(node->name), name) == 0;
}

// The file type a link offers, or empty when the reader cannot open it. Lower numbers are better.
std::string extension_for(const std::string &mime_in, const std::string &href, int &rank)
{
    const std::string mime = lower(mime_in);
    std::string path = lower(href);
    const size_t cut = path.find_first_of("?#");
    if (cut != std::string::npos)
    {
        path.resize(cut);
    }
    auto ends = [&](const char *suffix) {
        const size_t n = std::strlen(suffix);
        return path.size() >= n && path.compare(path.size() - n, n, suffix) == 0;
    };
    if (mime.find("epub") != std::string::npos || ends(".epub"))
    {
        rank = 0;
        return "epub";
    }
    if (mime.find("cbz") != std::string::npos || mime.find("comicbook") != std::string::npos || ends(".cbz"))
    {
        rank = 1;
        return "cbz";
    }
    if (mime.find("fictionbook") != std::string::npos || mime == "application/x-fb2+xml" || ends(".fb2"))
    {
        rank = 2;
        return "fb2";
    }
    if (mime == "text/markdown" || ends(".md"))
    {
        rank = 3;
        return "md";
    }
    if (mime == "text/plain" || ends(".txt"))
    {
        rank = 4;
        return "txt";
    }
    if (mime == "text/html" || mime == "application/xhtml+xml" || ends(".html") || ends(".htm"))
    {
        rank = 5;
        return "html";
    }
    return std::string();
}

} // namespace

Url parse_url(const std::string &input)
{
    Url url;
    std::string rest = trim(input);
    if (rest.empty())
    {
        url.error = "The address is empty";
        return url;
    }
    const std::string lowered = lower(rest);
    if (starts_with(lowered, "https://"))
    {
        url.error = "This address uses https, which the reader cannot open. Try the http:// address of the server.";
        return url;
    }
    if (starts_with(lowered, "http://"))
    {
        rest = rest.substr(7);
    }
    else if (lowered.find("://") != std::string::npos)
    {
        url.error = "Only http:// addresses work";
        return url;
    }
    const size_t hash = rest.find('#');
    if (hash != std::string::npos)
    {
        rest.resize(hash);
    }
    const size_t path_start = rest.find_first_of("/?");
    std::string authority = path_start == std::string::npos ? rest : rest.substr(0, path_start);
    if (path_start == std::string::npos)
    {
        url.path = "/";
    }
    else if (rest[path_start] == '?')
    {
        url.path = "/" + rest.substr(path_start);
    }
    else
    {
        url.path = rest.substr(path_start);
    }
    const size_t at = authority.rfind('@');
    if (at != std::string::npos)
    {
        url.credentials = authority.substr(0, at);
        authority = authority.substr(at + 1);
    }
    std::string port;
    if (!authority.empty() && authority[0] == '[')
    {
        const size_t close = authority.find(']');
        if (close == std::string::npos)
        {
            url.error = "The address is not valid";
            return url;
        }
        url.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size() && authority[close + 1] == ':')
        {
            port = authority.substr(close + 2);
        }
    }
    else
    {
        const size_t colon = authority.rfind(':');
        if (colon != std::string::npos)
        {
            url.host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
        }
        else
        {
            url.host = authority;
        }
    }
    if (url.host.empty())
    {
        url.error = "The address has no server name";
        return url;
    }
    if (!port.empty())
    {
        for (char c : port)
        {
            if (c < '0' || c > '9')
            {
                url.error = "The port in the address is not a number";
                return url;
            }
        }
        url.port = port;
    }
    url.ok = true;
    return url;
}

std::string resolve(const std::string &base, const std::string &href_in)
{
    const std::string href = trim(href_in);
    const std::string lowered = lower(href);
    if (starts_with(lowered, "http://") || starts_with(lowered, "https://"))
    {
        return href;
    }
    // Split the base into "http://authority" and its path
    std::string origin = "http://";
    std::string path = "/";
    {
        std::string rest = base;
        const size_t scheme = rest.find("://");
        if (scheme != std::string::npos)
        {
            origin = rest.substr(0, scheme + 3);
            rest = rest.substr(scheme + 3);
        }
        const size_t slash = rest.find_first_of("/?#");
        if (slash == std::string::npos)
        {
            origin += rest;
        }
        else
        {
            origin += rest.substr(0, slash);
            path = rest.substr(slash);
            if (path[0] != '/')
            {
                path = "/" + path;
            }
        }
    }
    if (starts_with(href, "//"))
    {
        return "http:" + href;
    }
    if (href.empty())
    {
        return origin + path;
    }
    const size_t cut = path.find_first_of("#");
    if (cut != std::string::npos)
    {
        path.resize(cut);
    }
    if (href[0] == '#')
    {
        return origin + path;
    }
    if (href[0] == '?')
    {
        const size_t q = path.find('?');
        return origin + (q == std::string::npos ? path : path.substr(0, q)) + href;
    }
    if (href[0] == '/')
    {
        return origin + href;
    }
    // A path relative to the folder of the base
    const size_t q = path.find('?');
    if (q != std::string::npos)
    {
        path.resize(q);
    }
    const size_t last = path.rfind('/');
    std::string joined = path.substr(0, last + 1) + href;
    // Remove "." and ".." parts of the path, but not the query
    std::string query;
    const size_t hq = joined.find('?');
    if (hq != std::string::npos)
    {
        query = joined.substr(hq);
        joined.resize(hq);
    }
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= joined.size())
    {
        size_t end = joined.find('/', start);
        if (end == std::string::npos)
        {
            end = joined.size();
        }
        const std::string part = joined.substr(start, end - start);
        if (part == "..")
        {
            if (parts.size() > 1)
            {
                parts.pop_back();
            }
        }
        else if (part != ".")
        {
            parts.push_back(part);
        }
        start = end + 1;
    }
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        out += (i == 0 ? "" : "/") + parts[i];
    }
    return origin + out + query;
}

bool ChunkDecoder::feed(const char *data, size_t length, std::string &out)
{
    size_t i = 0;
    while (i < length && state != State::Done)
    {
        switch (state)
        {
            case State::Size:
            {
                const char c = data[i++];
                if (c == '\n')
                {
                    const size_t semi = line.find(';');
                    const std::string digits = trim(semi == std::string::npos ? line : line.substr(0, semi));
                    line.clear();
                    if (digits.empty())
                    {
                        return false;
                    }
                    char *end = nullptr;
                    const unsigned long long size = std::strtoull(digits.c_str(), &end, 16);
                    if (!end || *end != '\0')
                    {
                        return false;
                    }
                    remaining = static_cast<size_t>(size);
                    state = remaining == 0 ? State::Trailer : State::Data;
                }
                else if (c != '\r')
                {
                    line.push_back(c);
                    if (line.size() > 64)
                    {
                        return false;
                    }
                }
                break;
            }
            case State::Data:
            {
                const size_t take = std::min(remaining, length - i);
                out.append(data + i, take);
                i += take;
                remaining -= take;
                if (remaining == 0)
                {
                    state = State::DataEnd;
                }
                break;
            }
            case State::DataEnd:
            {
                const char c = data[i++];
                if (c == '\n')
                {
                    state = State::Size;
                }
                else if (c != '\r')
                {
                    return false;
                }
                break;
            }
            case State::Trailer:
            {
                // After the last chunk comes optional headers and an empty line
                const char c = data[i++];
                if (c == '\n')
                {
                    if (line.empty())
                    {
                        state = State::Done;
                    }
                    line.clear();
                }
                else if (c != '\r')
                {
                    line.push_back(c);
                }
                break;
            }
            case State::Done:
                break;
        }
    }
    return true;
}

void set_timeout(int seconds)
{
    g_timeout_seconds = std::max(1, seconds);
}

bool call(
    const std::string &method,
    const std::string &url,
    const std::vector<std::pair<std::string, std::string>> &headers,
    const std::string &body,
    Response &response,
    std::string &error
)
{
    response = Response();
    RequestOptions options;
    options.method = method;
    options.headers = headers;
    options.body = body;
    options.status = &response.status;
    return request(
        url,
        [&response](const char *data, size_t len) {
            if (response.body.size() + len > MAX_PAGE_BYTES)
            {
                return false;
            }
            response.body.append(data, len);
            return true;
        },
        nullptr,
        error,
        options
    );
}

bool fetch(const std::string &url, std::string &body, std::string &error, const Progress &progress)
{
    body.clear();
    return request(
        url,
        [&body](const char *data, size_t len) {
            if (body.size() + len > MAX_PAGE_BYTES)
            {
                return false;
            }
            body.append(data, len);
            return true;
        },
        progress,
        error
    );
}

bool download(const std::string &url, const std::filesystem::path &file, std::string &error, const Progress &progress)
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::filesystem::path part = file;
    part += ".part";
    bool ok;
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "Could not write " + file.string();
            return false;
        }
        ok = request(
            url,
            [&out](const char *data, size_t len) {
                out.write(data, static_cast<std::streamsize>(len));
                return static_cast<bool>(out);
            },
            progress,
            error
        );
        out.flush();
        if (ok && !out)
        {
            error = "The card is full or not writable";
            ok = false;
        }
    }
    if (!ok)
    {
        std::filesystem::remove(part, ec);
        return false;
    }
    std::filesystem::rename(part, file, ec);
    if (ec)
    {
        std::filesystem::remove(part, ec);
        error = "Could not save the file";
        return false;
    }
    return true;
}

bool parse_feed(const std::string &xml, const std::string &base, Feed &feed, std::string &error)
{
    feed = Feed();
    xmlDoc *doc = xmlReadMemory(
        xml.data(),
        static_cast<int>(xml.size()),
        "catalog.xml",
        nullptr,
        XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET | XML_PARSE_RECOVER
    );
    if (!doc)
    {
        error = "The server did not send a catalog";
        return false;
    }
    xmlNode *root = xmlDocGetRootElement(doc);
    if (!root || !is_named(root, "feed"))
    {
        xmlFreeDoc(doc);
        error = "This address is not an OPDS catalog";
        return false;
    }
    for (xmlNode *node = root->children; node; node = node->next)
    {
        if (is_named(node, "title"))
        {
            feed.title = text_of(node);
        }
        else if (is_named(node, "link"))
        {
            if (attr_of(node, "rel") == "next")
            {
                feed.next = resolve(base, attr_of(node, "href"));
            }
        }
        else if (is_named(node, "entry"))
        {
            Entry entry;
            std::string catalog_href;
            std::string book_href;
            std::string book_extension;
            int best_rank = 99;
            for (xmlNode *child = node->children; child; child = child->next)
            {
                if (is_named(child, "title"))
                {
                    entry.title = text_of(child);
                }
                else if (is_named(child, "author"))
                {
                    for (xmlNode *name = child->children; name; name = name->next)
                    {
                        if (is_named(name, "name"))
                        {
                            const std::string who = text_of(name);
                            if (!who.empty())
                            {
                                entry.author += (entry.author.empty() ? "" : ", ") + who;
                            }
                        }
                    }
                }
                else if (is_named(child, "link"))
                {
                    const std::string rel = attr_of(child, "rel");
                    const std::string type = attr_of(child, "type");
                    const std::string href = attr_of(child, "href");
                    if (href.empty())
                    {
                        continue;
                    }
                    if (starts_with(rel, "http://opds-spec.org/acquisition"))
                    {
                        int rank = 99;
                        const std::string extension = extension_for(type, href, rank);
                        if (!extension.empty() && rank < best_rank)
                        {
                            best_rank = rank;
                            book_href = href;
                            book_extension = extension;
                        }
                    }
                    else if (lower(type).find("atom+xml") != std::string::npos && rel != "self" && rel != "search"
                        && rel != "next" && rel != "previous" && rel != "first" && rel != "last" && rel != "up"
                        && rel != "start" && rel != "alternate" && catalog_href.empty())
                    {
                        catalog_href = href;
                    }
                }
            }
            if (entry.title.empty())
            {
                entry.title = "(untitled)";
            }
            if (!book_href.empty())
            {
                entry.is_book = true;
                entry.href = resolve(base, book_href);
                entry.extension = book_extension;
                feed.entries.push_back(entry);
            }
            else if (!catalog_href.empty())
            {
                entry.href = resolve(base, catalog_href);
                feed.entries.push_back(entry);
            }
        }
    }
    xmlFreeDoc(doc);
    return true;
}

std::string file_name_for(const std::string &title, const std::string &author, const std::string &extension)
{
    std::string name = title;
    if (!author.empty())
    {
        name += " - " + author;
    }
    std::string clean;
    for (unsigned char c : name)
    {
        if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            clean += '_';
        }
        else
        {
            clean += static_cast<char>(c);
        }
    }
    // Keep the name short, without cutting a character in half
    constexpr size_t MAX_NAME = 90;
    if (clean.size() > MAX_NAME)
    {
        size_t cut = MAX_NAME;
        while (cut > 0 && (static_cast<unsigned char>(clean[cut]) & 0xC0) == 0x80)
        {
            --cut;
        }
        clean.resize(cut);
    }
    clean = trim(clean);
    while (!clean.empty() && clean.back() == '.')
    {
        clean.pop_back();
    }
    if (clean.empty())
    {
        clean = "book";
    }
    return clean + "." + extension;
}

} // namespace opds
