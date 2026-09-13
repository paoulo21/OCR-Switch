#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace switch_ocr {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool contains(int px, int py) const {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }

    int distance_squared_to(int px, int py) const {
        int cx = x + w / 2;
        int cy = y + h / 2;
        int dx = cx - px;
        int dy = cy - py;
        return dx * dx + dy * dy;
    }
};

struct TokenMatch {
    std::string word;
    std::string reading;
    std::vector<std::string> definitions;
    std::vector<std::string> pitch;
    int char_start = 0;
    int char_end = 0;
};

struct DetectedBox {
    Rect rect;
    std::string text;
    float confidence = 1.0f;
    std::vector<TokenMatch> tokens;
};

struct OCRResponse {
    bool success = false;
    std::string error_message;
    std::vector<DetectedBox> boxes;
};

struct CursorState {
    int x = 640;
    int y = 360;
    bool snapped = false;
    int active_box_idx = -1;
    int active_token_idx = -1;
    int current_def_page = 0;
};

struct Config {
    std::string server_ip = "192.168.1.100";
    int server_port = 8766;
    int discovery_port = 8764;
    bool auto_discovery = true;
    int cursor_speed = 8;
    int snap_radius = 60;
};

} // namespace switch_ocr
