#ifndef WIFI_UPLOAD_H_
#define WIFI_UPLOAD_H_

#include <filesystem>
#include <string>
#include <vector>

// A tiny web page served from the handheld, so books can be sent from a phone or
// a computer on the same wifi without taking the card out. It speaks plain HTTP,
// takes one request at a time, and only runs while its screen is open.
class UploadServer
{
    int listener = -1;
    int port = 0;
    std::filesystem::path folder;
    std::vector<std::string> received; // file names saved since it started
    std::string last_message;

    void handle(int client);

public:
    explicit UploadServer(std::filesystem::path folder);
    ~UploadServer();
    UploadServer(const UploadServer &) = delete;
    UploadServer &operator=(const UploadServer &) = delete;

    // Starts listening on the first free port from first_port. False with a message when it cannot.
    bool start(int first_port, std::string &error);
    void stop();
    bool running() const { return listener >= 0; }
    int get_port() const { return port; }

    // Serves at most one waiting request. True when something changed on screen.
    bool poll();

    const std::vector<std::string> &files() const { return received; }
    const std::string &message() const { return last_message; }
};

// The address of this device on the local network, such as "192.168.1.20". Empty when there is none.
std::string local_ip_address();

// A file name from an upload made safe for the card: no folders, no odd characters.
// Empty when the file is not a type the reader opens.
std::string upload_safe_name(const std::string &raw);

#endif
