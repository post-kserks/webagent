#pragma once
#include <string>

class Agent {
public:
    Agent(const std::string& config_file);
    void run();

private:
    std::string uid_;
    std::string server_uri_;
    int interval_;
    std::string access_code_;
    
    void log(const std::string& msg);
    std::string http_post(const std::string& url, const std::string& data);
};