#include "../include/video_loader.h"
#include <cstdlib>
#include <string>
#include <filesystem>

bool load_vid(const std::string& github_url, const std::string& filename) {
    std::string raw_url = github_url;
    
    size_t pos = raw_url.find("github.com");
    if (pos != std::string::npos)
        raw_url.replace(pos, 10, "raw.githubusercontent.com");
    
    pos = raw_url.find("/blob/");
    if (pos != std::string::npos)
        raw_url.erase(pos, 5);
    
    std::string command;

    #ifdef _WIN32
        // On Windows, curl is generally available in modern versions 
        // (Windows 10 1803+). 
        command = "curl -L -o " + filename + " " + raw_url;
    #else
        // On Unix-like systems, check for curl first, then wget
        if (system("which curl > /dev/null 2>&1") == 0) {
            command = "curl -L -o " + filename + " " + raw_url;
        } else if (system("which wget > /dev/null 2>&1") == 0) {
            command = "wget -O " + filename + " " + raw_url;
        } else {
            // No downloader found
            return false;
        }
    #endif
    
    return system(command.c_str()) == 0;
}
bool clear_resources() {
    #ifdef _WIN32
        return system("del /Q screamer.mp4 2>nul") == 0;
    #elif defined(__APPLE__)
        // macOS uses the same filesystem operations as Linux
        if (std::filesystem::exists("screamer.mp4")) {
            return std::filesystem::remove("screamer.mp4");
        }
        return true;
    #else
        if (std::filesystem::exists("screamer.mp4")) {
            return std::filesystem::remove("screamer.mp4");
        }
        return true;
    #endif
}
