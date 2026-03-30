#pragma once
#include <memory>
#include <mutex>
#include <string>

class LocalControlServer;

class Agent {
public:
    Agent(const std::string& config_file);
    ~Agent();
    void run();

private:
    std::string uid_;
    std::string descr_;
    std::string server_uri_;
    int interval_;
    int local_control_port_;
    std::string access_code_;
    std::string selected_video_;
    std::unique_ptr<LocalControlServer> local_control_server_;
    mutable std::mutex selected_video_mutex_;
    
    void log(const std::string& msg);
    std::string http_post(const std::string& url, const std::string& data);
    bool set_selected_video(const std::string& raw_value, const std::string& source);
    std::string get_selected_video() const;
    void init_local_control_server();
};
