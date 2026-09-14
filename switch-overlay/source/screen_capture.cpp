#include "screen_capture.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switch_ocr {

static bool s_initialized = false;

bool ScreenCapture::initialize() {
#ifdef __SWITCH__
    Result rc = capsscInitialize();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
        return true;
    }
    return false;
#else
    s_initialized = true;
    return true;
#endif
}

void ScreenCapture::exit() {
#ifdef __SWITCH__
    if (s_initialized) {
        capsscExit();
        s_initialized = false;
    }
#else
    s_initialized = false;
#endif
}

bool ScreenCapture::captureJpeg(std::vector<uint8_t>& outJpegBuffer) {
#ifdef __SWITCH__
    if (!s_initialized) {
        if (!initialize()) return false;
    }

    // Allocate buffer for 720p/1080p JPEG screenshot (typically <= 350KB)
    constexpr size_t MAX_JPEG_SIZE = 512 * 1024;
    outJpegBuffer.resize(MAX_JPEG_SIZE);

    u64 actualSize = 0;
    // Prefer capturing application layer directly (excludes Tesla overlay and OS menus)
    Result rc = capsscCaptureJpegScreenShot(&actualSize, outJpegBuffer.data(), MAX_JPEG_SIZE, ViLayerStack_ApplicationForScreenshots, 100000000LL);
    if (R_FAILED(rc) || actualSize == 0) {
        // Fallback to default composite layer
        rc = capsscCaptureJpegScreenShot(&actualSize, outJpegBuffer.data(), MAX_JPEG_SIZE, ViLayerStack_Default, 100000000LL);
    }
    if (R_SUCCEEDED(rc) && actualSize > 0) {
        outJpegBuffer.resize(actualSize);
        return true;
    }

    outJpegBuffer.clear();
    return false;
#else
    // Mock JPEG header for host/development compilation
    outJpegBuffer = {
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00, 0x01,
        0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
        0x00, 0xFF, 0xD9
    };
    return true;
#endif
}

} // namespace switch_ocr
