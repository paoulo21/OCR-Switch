#pragma once

#include "types.hpp"
#include <string>
#include <vector>

#ifdef __SWITCH__
#include <tesla.hpp>
#endif

namespace switch_ocr {

enum class OverlayState {
    IDLE,
    SCANNING,
    READY,
    ERROR,
    MINING_NOTIFICATION
};

class OverlayGui {
public:
    OverlayGui();
    ~OverlayGui();

    void init();
    void update(uint64_t keysDown, uint64_t keysHeld, int touchX, int touchY, bool touching);
    void render();

    void triggerScan();
    void triggerAnkiMining();

    OverlayState getState() const { return m_state; }

private:
    void updateCursorPosition(int dx, int dy);
    void checkSnapping();
    void selectTokenAt(int x, int y);

private:
    Config m_config;
    OverlayState m_state = OverlayState::IDLE;
    OCRResponse m_ocrData;
    CursorState m_cursor;
    std::string m_statusMessage;
    int m_notificationTimer = 0;
};

} // namespace switch_ocr
