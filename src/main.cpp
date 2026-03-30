#include "agent.h"
#include "frontend_launcher.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static std::string find_existing_path(
    const std::initializer_list<std::string>& candidates
) {
    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return *candidates.begin();
}

int main(int argc, char* argv[]) {
    try {
        bool open_frontend = true;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--no-frontend") {
                open_frontend = false;
            }
        }

        const std::string frontend_path = find_existing_path(
            {"../frontend/index.html", "frontend/index.html", "../../frontend/index.html"}
        );
        const std::string config = find_existing_path(
            {"../config/config.json", "config/config.json", "../../config/config.json"}
        );

        if (open_frontend) {
            if (!launch_frontend(frontend_path)) {
                std::cerr << "Warning: failed to open frontend at " << frontend_path << std::endl;
            }
        }

        Agent agent(config);
        agent.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
