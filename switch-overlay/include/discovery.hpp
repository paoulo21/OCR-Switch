#pragma once

#include <string>

namespace switch_ocr {

class DiscoveryClient {
public:
    static bool discoverServer(int discoveryPort, std::string& outIp, int& outPort, int timeoutMs = 1500);
};

} // namespace switch_ocr
