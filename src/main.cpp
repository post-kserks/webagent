#include "agent.h"
#include <iostream>
#include <fstream>

int main() {
    try {
        std::string config = "../config/config.json";
        Agent agent(config);
        agent.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}