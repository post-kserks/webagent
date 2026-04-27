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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;
using std::string;
using std::cout;
namespace fs = std::filesystem;

// ─────────────────────────── anonymous helpers ────────────────────────────
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

string shell_escape_single_quotes(const string& value) {
    string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('\'');
    for (char ch : value) {
        if (ch == '\'') escaped += "'\\''";
        else            escaped.push_back(ch);
    }
    escaped.push_back('\'');
    return escaped;
}

// Ensure directory exists (creates it if missing)
void ensure_dir(const string& path) {
    if (path.empty()) return;
    std::error_code ec;
    fs::create_directories(path, ec);
}

bool save_last_selected_video(const string& video_name) {
    const string normalized = agent_video_utils::extract_video_name_token(video_name);
    if (normalized.empty()) return false;
    std::ofstream f(kLastSelectedVideoFile, std::ios::trunc);
    if (!f.is_open()) return false;
    f << normalized;
    return true;
}

bool load_last_selected_video(string& out) {
    std::ifstream f(kLastSelectedVideoFile);
    if (!f.is_open()) return false;
    string cached;
    std::getline(f, cached);
    const string normalized = agent_video_utils::extract_video_name_token(cached);
    if (normalized.empty()) return false;
    out = normalized;
    return true;
}

} // namespace

// ─────────────────────────── logging ──────────────────────────────────────
void Agent::log(const string& msg) {
    std::ofstream(log_file_, std::ios::app) << msg << std::endl;
    cout << msg << std::endl;
}

// ─────────────────────────── HTTP ─────────────────────────────────────────
string Agent::http_post(const string& url, const string& data) {
    // Prepend PATH_ENV so curl/wget can be found even in restricted environments
    const string path_prefix = path_env_.empty()
        ? ""
        : "PATH=" + shell_escape_single_quotes(path_env_) + ":$PATH ";

    const string cmd =
        path_prefix +
        "curl -s -k -X POST -H 'Content-Type: application/json' --data-binary " +
        shell_escape_single_quotes(data) + " " +
        shell_escape_single_quotes(url);

    string result;
    FILE* pipe = platform_popen(cmd.c_str(), "r");
    if (!pipe) return result;

    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    platform_pclose(pipe);
    return result;
}

// ─────────────────────────── file upload ──────────────────────────────────
bool Agent::upload_file(const string& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) {
        log("Error: cannot open file for upload: " + filepath);
        return false;
    }
    const string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );
    json req = {
        {"UID",         uid_},
        {"access_code", access_code_},
        {"session_id",  session_id_},
        {"file_name",   fs::path(filepath).filename().string()},
        {"file_data",   content}
    };
    const string resp = http_post(server_uri_ + "wa_upload/", req.dump());
    log("Upload [" + filepath + "] response: " + resp);
    return !resp.empty();
}

// Upload every file found in results_folder_
bool Agent::upload_results_folder() {
    if (results_folder_.empty()) return true;
    std::error_code ec;
    if (!fs::exists(results_folder_, ec)) {
        log("Results folder does not exist: " + results_folder_);
        return false;
    }
    bool ok = true;
    for (const auto& entry : fs::directory_iterator(results_folder_, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_regular_file()) continue;
        if (!upload_file(entry.path().string())) ok = false;
    }
    return ok;
}

// Send execution result (code + optional message) back to server
bool Agent::send_task_result(int exec_code, const string& message) {
    json req = {
        {"UID",         uid_},
        {"access_code", access_code_},
        {"session_id",  session_id_},
        {"exec_code",   exec_code},
        {"message",     message}
    };
    const string resp = http_post(server_uri_ + "wa_result/", req.dump());
    log("Task result response: " + resp);
    return !resp.empty();
}

// ─────────────────────────── registration ─────────────────────────────────
bool Agent::register_on_server() {
    log("Registering on server...");
    json req = {{"UID", uid_}, {"descr", descr_}};
    const string resp = http_post(server_uri_ + "wa_reg/", req.dump());
    log("Registration response: " + resp);
    if (resp.empty()) return false;
    try {
        const auto j = json::parse(resp);
        if (j.contains("access_code")) {
            access_code_ = j["access_code"].get<string>();
            log("Access code received: " + access_code_);
            std::ofstream(".access_code_" + uid_) << access_code_;
            log("Access code saved.");
            return true;
        }
        if (j.contains("msg")) log("Registration refused: " + j["msg"].get<string>());
    } catch (...) {
        log("Failed to parse registration response.");
    }
    return false;
}

// ─────────────────────────── video helpers ────────────────────────────────
bool Agent::set_selected_video(const string& raw_value, const string& source) {
    string normalized;
    if (!agent_video_utils::normalize_video_name_for_control(raw_value, normalized)) {
        log("Rejected selected video from " + source + ": " + raw_value);
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(selected_video_mutex_);
        selected_video_ = normalized;
    }
    if (!save_last_selected_video(normalized))
        log("Warning: failed to persist selected video: " + normalized);
    log("Selected video updated from " + source + ": " + normalized);
    return true;
}

string Agent::get_selected_video() const {
    std::lock_guard<std::mutex> lock(selected_video_mutex_);
    return selected_video_;
}

// ─────────────────────────── local control server ─────────────────────────
void Agent::init_local_control_server() {
    local_control_server_ = std::make_unique<LocalControlServer>(
        local_control_port_,
        [this](const string& value) { return set_selected_video(value, "frontend-api"); },
        [this]()                    { return get_selected_video(); },
        [this](const string& msg)   { log(msg); }
    );
    if (!local_control_server_->start())
        log("Warning: local control server did not start.");
}

// ─────────────────────────── constructor ──────────────────────────────────
Agent::Agent(const string& config_file)
    : log_file_("agent.log"),
      tasks_folder_("./tasks"),
      results_folder_("./results"),
      path_env_(""),
      interval_(5),
      max_retry_interval_(60),
      local_control_port_(8787),
      session_id_(""),
      current_interval_(5),
      selected_video_("screamer1.mp4")
{
    std::ifstream f(config_file);
    if (!f.is_open())
        throw std::runtime_error("Cannot open config file: " + config_file);

    json cfg = json::parse(f);
    uid_               = cfg.value("uid",               "");
    descr_             = cfg.value("descr",             "web-agent");
    server_uri_        = cfg.value("server_uri",        "");
    log_file_          = cfg.value("log_file",          "agent.log");
    tasks_folder_      = cfg.value("tasks_folder",      "./tasks");
    results_folder_    = cfg.value("results_folder",    "./results");
    path_env_          = cfg.value("path_env",          "");
    interval_          = std::max(1, cfg.value("request_interval",  5));
    max_retry_interval_= std::max(1, cfg.value("max_retry_interval", 60));
    local_control_port_= std::max(1, cfg.value("local_control_port", 8787));
    current_interval_  = interval_;

    if (uid_.empty() || server_uri_.empty())
        throw std::runtime_error("Config must contain non-empty uid and server_uri");

    // Ensure required directories exist
    ensure_dir(tasks_folder_);
    ensure_dir(results_folder_);
    ensure_dir(fs::path(log_file_).parent_path().string());

    // Truncate log at startup
    std::ofstream(log_file_, std::ios::trunc).close();

    string cached_video;
    if (load_last_selected_video(cached_video))
        selected_video_ = cached_video;

    log("Agent started, UID: " + uid_);
    log("Log file:        " + log_file_);
    log("Tasks folder:    " + tasks_folder_);
    log("Results folder:  " + results_folder_);
    log("PATH env:        " + (path_env_.empty() ? "(system default)" : path_env_));
    log("Initial video:   " + selected_video_);

    // Try loading saved access code; otherwise register
    std::ifstream code_file(".access_code_" + uid_);
    if (code_file.is_open()) {
        std::getline(code_file, access_code_);
        log("Loaded saved access code: " + access_code_);
    } else {
        // Retry registration with backoff until success
        int reg_wait = interval_;
        while (!register_on_server()) {
            log("Registration failed. Retrying in " + std::to_string(reg_wait) + "s...");
            std::this_thread::sleep_for(std::chrono::seconds(reg_wait));
            reg_wait = std::min(reg_wait * 2, max_retry_interval_);
        }
    }

    init_local_control_server();
}

// ─────────────────────────── destructor ───────────────────────────────────
Agent::~Agent() {
    if (local_control_server_) local_control_server_->stop();
}

// ─────────────────────────── main loop ────────────────────────────────────
void Agent::run() {
    int poll_counter = 0;

    while (true) {
        ++poll_counter;
        log("--- Poll #" + std::to_string(poll_counter) +
            " (interval=" + std::to_string(current_interval_) + "s) ---");

        json req = {
            {"UID",         uid_},
            {"descr",       descr_},
            {"access_code", access_code_}
        };
        const string resp = http_post(server_uri_ + "wa_task/", req.dump());

        // ── Server unavailable → exponential backoff ──────────────────────
        if (resp.empty()) {
            log("Server unavailable. Next attempt in " +
                std::to_string(current_interval_) + "s.");
            std::this_thread::sleep_for(std::chrono::seconds(current_interval_));
            current_interval_ = std::min(current_interval_ * 2, max_retry_interval_);
            continue;
        }

        // Server responded → reset interval to configured value
        current_interval_ = interval_;

        // ── Parse response ────────────────────────────────────────────────
        try {
            auto task = json::parse(resp);
            const int code = std::stoi(task["code_responce"].get<string>());

            if (code == 1) {
                const string task_code = task.value("task_code", "");
                session_id_ = task.value("session_id", session_id_);

                log("Task received: " + task_code +
                    "  session=" + session_id_);

                // ── TASK: launch program / play video ─────────────────────
                if (task_code == "TASK") {
                    string video_name = get_selected_video();

                    if (task.contains("options")) {
                        const json& option = task["options"];
                        if (!agent_video_utils::is_empty_task_option(option)) {
                            const string resolved =
                                agent_video_utils::resolve_video_name(option);
                            set_selected_video(resolved, "task-options");
                            video_name = get_selected_video();
                            log("Video option: " + option.dump() +
                                " → " + video_name);
                        } else {
                            log("Empty video option; using: " + video_name);
                        }
                    }

                    const string video_url =
                        "https://github.com/testerVsego/vid_for_agent/blob/main/" +
                        video_name;

                    clear_resources();
                    int exec_code = 0;
                    string exec_msg = "ok";
                    if (load_vid(video_url, video_name)) {
                        // Move downloaded file to tasks_folder
                        std::error_code ec;
                        const string dest = tasks_folder_ + "/" + video_name;
                        fs::copy(video_name, dest,
                            fs::copy_options::overwrite_existing, ec);
                        if (ec) log("Warning: could not copy to tasks folder: " +
                                    ec.message());

                        zapusk_exe(video_name);
                    } else {
                        exec_code = 1;
                        exec_msg  = "Failed to load video: " + video_url;
                        log(exec_msg);
                    }
                    send_task_result(exec_code, exec_msg);

                // ── FILE: send results folder contents to server ──────────
                } else if (task_code == "FILE") {
                    // Always upload the agent log
                    upload_file(log_file_);
                    // Upload all result files
                    upload_results_folder();

                // ── CONF: stop or reconfigure ─────────────────────────────
                } else if (task_code == "CONF") {
                    log("CONF received. Stopping agent.");
                    break;

                // ── TIMEOUT: update polling interval ──────────────────────
                } else if (task_code == "TIMEOUT") {
                    try {
                        const string raw = task.value("options", "");
                        const int new_interval = std::max(1, std::stoi(raw));
                        interval_         = new_interval;
                        current_interval_ = new_interval;
                        log("Polling interval updated to " +
                            std::to_string(interval_) + "s.");
                    } catch (...) {
                        log("Invalid TIMEOUT options. Interval unchanged.");
                    }

                } else {
                    log("Unknown task code: " + task_code);
                }

            } else if (code == 0) {
                log("No tasks.");
            } else {
                log("Server error code: " + std::to_string(code));
                if (task.contains("msg"))
                    log("Server message: " + task["msg"].get<string>());
            }

        } catch (...) {
            log("Failed to parse task response: " + resp);
        }

        std::this_thread::sleep_for(std::chrono::seconds(current_interval_));
    }
}
