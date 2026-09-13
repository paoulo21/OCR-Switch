#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace switch_ocr {

class ScreenCapture {
public:
    static bool initialize();
    static void exit();
    static bool captureJpeg(std::vector<uint8_t>& outJpegBuffer);
};

} // namespace switch_ocr
