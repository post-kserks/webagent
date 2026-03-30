#include "frontend_launcher.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

bool launch_frontend(const std::string& frontend_path) {
    const fs::path path = fs::absolute(frontend_path);
    if (!fs::exists(path)) {
        return false;
    }

#ifdef _WIN32
    const auto result = ShellExecuteA(
        nullptr,
        "open",
        path.string().c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<intptr_t>(result) > 32;
#elif __APPLE__
    const std::string command = "open \"" + path.string() + "\" >/dev/null 2>&1 &";
    return std::system(command.c_str()) == 0;
#else
    std::string command;
    if (std::system("which wslview > /dev/null 2>&1") == 0) {
        command = "wslview \"" + path.string() + "\" >/dev/null 2>&1 &";
    } else {
        command = "xdg-open \"" + path.string() + "\" >/dev/null 2>&1 &";
    }
    return std::system(command.c_str()) == 0;
#endif
}
