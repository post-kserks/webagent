#include "../include/video_loader.h"
#include <cctype>
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
    auto is_screamer_video = [](const std::filesystem::path& p) {
        const std::string name = p.filename().string();
        if (name == "screamer.mp4") {
            return true;
        }

        const std::string prefix = "screamer";
        const std::string suffix = ".mp4";

        if (name.size() <= prefix.size() + suffix.size()) {
            return false;
        }
        if (name.rfind(prefix, 0) != 0) {
            return false;
        }
        if (name.substr(name.size() - suffix.size()) != suffix) {
            return false;
        }

        const std::string middle = name.substr(
            prefix.size(),
            name.size() - prefix.size() - suffix.size()
        );
        if (middle.empty()) {
            return false;
        }

        for (unsigned char ch : middle) {
            if (!std::isdigit(ch)) {
                return false;
            }
        }
        return true;
    };

    bool ok = true;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (ec) {
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!is_screamer_video(entry.path())) {
            continue;
        }
        std::filesystem::remove(entry.path(), ec);
        if (ec) {
            ok = false;
            ec.clear();
        }
    }

    return ok;
}
