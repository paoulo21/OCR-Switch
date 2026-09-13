#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <arpa/inet.h>

namespace switch_ocr {

static inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

Config ConfigManager::load(const std::string& path) {
    Config cfg;
    std::ifstream file(path);
    if (!file.is_open()) {
        return cfg;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') {
            continue;
        }

        auto sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = trim(line.substr(0, sep));
        std::string val = trim(line.substr(sep + 1));

        // Strip inline comments (; or #)
        auto commentPos = val.find_first_of(";#");
        if (commentPos != std::string::npos) {
            val = trim(val.substr(0, commentPos));
        }

        if (key == "server_ip") {
            struct in_addr addr;
            if (inet_pton(AF_INET, val.c_str(), &addr) > 0) {
                cfg.server_ip = val;
            }
        } else if (key == "server_port") {
            cfg.server_port = std::atoi(val.c_str());
        } else if (key == "discovery_port") {
            cfg.discovery_port = std::atoi(val.c_str());
        } else if (key == "auto_discovery") {
            cfg.auto_discovery = (val == "true" || val == "1" || val == "yes");
        } else if (key == "cursor_speed") {
            cfg.cursor_speed = std::atoi(val.c_str());
        } else if (key == "snap_radius") {
            cfg.snap_radius = std::atoi(val.c_str());
        }
    }
    return cfg;
}

bool ConfigManager::save(const Config& config, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "[network]\n";
    file << "server_ip=" << config.server_ip << "\n";
    file << "server_port=" << config.server_port << "\n";
    file << "discovery_port=" << config.discovery_port << "\n";
    file << "auto_discovery=" << (config.auto_discovery ? "true" : "false") << "\n\n";

    file << "[controls]\n";
    file << "cursor_speed=" << config.cursor_speed << "\n";
    file << "snap_radius=" << config.snap_radius << "\n";

    return true;
}

} // namespace switch_ocr
