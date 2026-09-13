#pragma once

#include "types.hpp"

namespace switch_ocr {

class ConfigManager {
public:
    static Config load(const std::string& path = "sdmc:/config/switch-ocr/config.ini");
    static bool save(const Config& config, const std::string& path = "sdmc:/config/switch-ocr/config.ini");
};

} // namespace switch_ocr
