#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

class LocalControlServer {
public:
    using SetVideoHandler = std::function<bool(const std::string&)>;
    using GetVideoHandler = std::function<std::string()>;
    using LogHandler = std::function<void(const std::string&)>;

    LocalControlServer(
        int port,
        SetVideoHandler set_video_handler,
        GetVideoHandler get_video_handler,
        LogHandler log_handler
    );
    ~LocalControlServer();

    bool start();
    void stop();

private:
    int port_;
    SetVideoHandler set_video_handler_;
    GetVideoHandler get_video_handler_;
    LogHandler log_handler_;

    std::atomic<bool> running_;
    int server_fd_;
    std::thread server_thread_;

    void server_loop();
    void log(const std::string& message) const;
};
