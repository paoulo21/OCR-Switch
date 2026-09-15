#pragma once

#include "types.hpp"
#include <string>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#include <tesla.hpp>
#endif

namespace switch_ocr {

enum class OverlayState {
    IDLE,
    SCANNING,
    READY,
    ERROR,
    MINING_NOTIFICATION,
    SETTINGS
};

class OverlayGui {
public:
    OverlayGui();
    ~OverlayGui();

    void init();
    void update();

#ifdef __SWITCH__
    bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick);
    void render(tsl::gfx::Renderer* renderer, s32 frameX, s32 frameY, s32 frameW, s32 frameH);
#endif

    void triggerScan();
    void triggerAnkiMining();
    void openSettings();
    void closeSettings();

    OverlayState getState() const { return m_state; }

private:
    void updateCursorPosition(int dx, int dy);
    void checkSnapping();
    void selectTokenAt(int x, int y);
    void loadIpOctets();
    void saveIpOctets();

private:
    Config m_config;
    OverlayState m_state = OverlayState::IDLE;
    OCRResponse m_ocrData;
    CursorState m_cursor;
    std::string m_statusMessage;
    int m_notificationTimer = 0;

    // Server IP editor state
    int m_ipOctets[4] = { 192, 168, 1, 100 };
    int m_selectedOctet = 3; // 0..3 (defaults to 4th host octet)
    std::string m_settingsStatus;
};

} // namespace switch_ocr
