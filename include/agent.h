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
    // --- Config fields ---
    std::string uid_;
    std::string descr_;
    std::string server_uri_;
    std::string log_file_;
    std::string tasks_folder_;
    std::string results_folder_;
    std::string path_env_;
    int         interval_;
    int         max_retry_interval_;
    int         local_control_port_;

    // --- Runtime state ---
    std::string access_code_;
    std::string session_id_;
    int         current_interval_;
    std::string selected_video_;

    std::unique_ptr<LocalControlServer> local_control_server_;
    mutable std::mutex selected_video_mutex_;

    // --- Helpers ---
    void        log(const std::string& msg);
    std::string http_post(const std::string& url, const std::string& data);
    bool        upload_file(const std::string& filepath);
    bool        upload_results_folder();
    bool        send_task_result(int exec_code, const std::string& message);
    bool        register_on_server();

    bool        set_selected_video(const std::string& raw_value, const std::string& source);
    std::string get_selected_video() const;
    void        init_local_control_server();
};
