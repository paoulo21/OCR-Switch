#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>

namespace switch_ocr {

class HttpClient {
public:
    static OCRResponse performOcr(
        const std::string& serverIp,
        int serverPort,
        const uint8_t* jpegData,
        size_t jpegSize,
        int timeoutSec = 5
    );

    static bool exportAnki(
        const std::string& serverIp,
        int serverPort,
        const std::string& word,
        const std::string& reading,
        const std::vector<std::string>& definitions,
        const std::string& sentence,
        int timeoutSec = 3
    );
};

} // namespace switch_ocr
