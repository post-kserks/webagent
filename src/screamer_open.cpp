    #include "screamer_open.h"
    #include <cstdlib>
    #include <iostream>
    #include <filesystem>

    #ifdef _WIN32
        #include <windows.h>
    #elif __APPLE__
        #include <TargetConditionals.h>
    #endif

    namespace fs = std::filesystem;

    void zapusk_exe(const std::string& video_path) {
        #ifdef _WIN32
            ShellExecuteA(NULL, "open", video_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        #elif __APPLE__
            std::string command = "open \"" + video_path + "\" 2>/dev/null &";
            system(command.c_str());
        #else
            if (system("which wslview > /dev/null 2>&1") == 0) {
                std::string command = "wslview \"" + video_path + "\" 2>/dev/null &";
                system(command.c_str());
            } else {
                std::string command = "xdg-open \"" + video_path + "\" 2>/dev/null &";
                system(command.c_str());
            }
        #endif
    }

    void update_video(const std::string& source_path) {
        std::string dest_path = "./screamer.mp4";
        
        if (fs::exists(source_path)) {
            fs::copy(source_path, dest_path, fs::copy_options::overwrite_existing);
            std::cout << "Video updated: " << source_path << std::endl;
        } else {
            std::cerr << "Source video not found: " << source_path << std::endl;
        }
    }
