#include "gui/overlay_gui.hpp"
#include "config.hpp"
#include "network/discovery.hpp"
#include "network/http_client.hpp"
#include "capture/screen_capture.hpp"

#include <algorithm>
#include <cmath>

#ifdef __SWITCH__
#include <switch.h>
#include <tesla.hpp>
#endif

namespace switch_ocr {

OverlayGui::OverlayGui() {
    m_cursor.x = 640;
    m_cursor.y = 360;
}

OverlayGui::~OverlayGui() {
    ScreenCapture::exit();
}

void OverlayGui::init() {
    m_config = ConfigManager::load();

    // Run UDP auto-discovery if enabled
    if (m_config.auto_discovery) {
        std::string foundIp;
        int foundPort = 8766;
        if (DiscoveryClient::discoverServer(m_config.discovery_port, foundIp, foundPort, 1200)) {
            m_config.server_ip = foundIp;
            m_config.server_port = foundPort;
        }
    }

    ScreenCapture::initialize();
    triggerScan();
}

void OverlayGui::triggerScan() {
    m_state = OverlayState::SCANNING;
    m_statusMessage = "Capture de l'ecran et analyse OCR en cours...";

    std::vector<uint8_t> jpeg;
    if (!ScreenCapture::captureJpeg(jpeg)) {
        m_state = OverlayState::ERROR;
        m_statusMessage = "Erreur: Impossible de capturer l'ecran.";
        return;
    }

    m_ocrData = HttpClient::performOcr(m_config.server_ip, m_config.server_port, jpeg.data(), jpeg.size());
    if (!m_ocrData.success) {
        m_state = OverlayState::ERROR;
        m_statusMessage = m_ocrData.error_message.empty() ? "Erreur de connexion au serveur OCR." : m_ocrData.error_message;
        return;
    }

    m_state = OverlayState::READY;
    m_statusMessage = "";
    m_cursor.active_box_idx = -1;
    m_cursor.active_token_idx = -1;
    m_cursor.current_def_page = 0;

    // Automatically snap to first detected box if present
    if (!m_ocrData.boxes.empty()) {
        const auto& first = m_ocrData.boxes[0];
        m_cursor.x = first.rect.x + first.rect.w / 2;
        m_cursor.y = first.rect.y + first.rect.h / 2;
        m_cursor.active_box_idx = 0;
        if (!first.tokens.empty()) {
            m_cursor.active_token_idx = 0;
        }
    }
}

void OverlayGui::triggerAnkiMining() {
    if (m_cursor.active_box_idx < 0 || m_cursor.active_box_idx >= (int)m_ocrData.boxes.size()) {
        return;
    }
    const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
    if (box.tokens.empty() || m_cursor.active_token_idx < 0 || m_cursor.active_token_idx >= (int)box.tokens.size()) {
        return;
    }

    const auto& token = box.tokens[m_cursor.active_token_idx];
    bool ok = HttpClient::exportAnki(
        m_config.server_ip,
        m_config.server_port,
        token.word,
        token.reading,
        token.definitions,
        box.text
    );

    m_state = OverlayState::MINING_NOTIFICATION;
    m_statusMessage = ok ? "Mot exporte vers Anki avec succes !" : "Echec de l'export Anki (verifiez le serveur).";
    m_notificationTimer = 120; // Show toast for ~2 seconds
}

void OverlayGui::updateCursorPosition(int dx, int dy) {
    m_cursor.x = std::clamp(m_cursor.x + dx, 0, 1280);
    m_cursor.y = std::clamp(m_cursor.y + dy, 0, 720);
    checkSnapping();
}

void OverlayGui::checkSnapping() {
    int bestDistSq = m_config.snap_radius * m_config.snap_radius;
    int closestBox = -1;

    for (size_t i = 0; i < m_ocrData.boxes.size(); ++i) {
        const auto& b = m_ocrData.boxes[i];
        if (b.rect.contains(m_cursor.x, m_cursor.y)) {
            closestBox = (int)i;
            break;
        }
        int d = b.rect.distance_squared_to(m_cursor.x, m_cursor.y);
        if (d < bestDistSq) {
            bestDistSq = d;
            closestBox = (int)i;
        }
    }

    if (closestBox != m_cursor.active_box_idx) {
        m_cursor.active_box_idx = closestBox;
        m_cursor.active_token_idx = (closestBox >= 0 && !m_ocrData.boxes[closestBox].tokens.empty()) ? 0 : -1;
        m_cursor.current_def_page = 0;
    }
}

void OverlayGui::selectTokenAt(int x, int y) {
    m_cursor.x = x;
    m_cursor.y = y;
    checkSnapping();
}

void OverlayGui::update(uint64_t keysDown, uint64_t keysHeld, int touchX, int touchY, bool touching) {
    if (m_state == OverlayState::MINING_NOTIFICATION) {
        if (--m_notificationTimer <= 0) {
            m_state = OverlayState::READY;
        }
    }

    // Touch screen handling (handheld mode)
    if (touching && touchX >= 0 && touchY >= 0) {
        selectTokenAt(touchX, touchY);
    }

    // D-Pad and Left Stick cursor movement
    int dx = 0;
    int dy = 0;
    int speed = m_config.cursor_speed;

#ifdef __SWITCH__
    if (keysHeld & HidNpadButton_StickLLeft || keysHeld & HidNpadButton_DLeft) dx -= speed;
    if (keysHeld & HidNpadButton_StickLRight || keysHeld & HidNpadButton_DRight) dx += speed;
    if (keysHeld & HidNpadButton_StickLUp || keysHeld & HidNpadButton_DUp) dy -= speed;
    if (keysHeld & HidNpadButton_StickLDown || keysHeld & HidNpadButton_DDown) dy += speed;

    if (dx != 0 || dy != 0) {
        updateCursorPosition(dx, dy);
    }

    // Button A or R: cycle definitions / next token
    if ((keysDown & HidNpadButton_A) || (keysDown & HidNpadButton_R)) {
        if (m_cursor.active_box_idx >= 0 && m_cursor.active_box_idx < (int)m_ocrData.boxes.size()) {
            const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
            if (box.tokens.size() > 1) {
                m_cursor.active_token_idx = (m_cursor.active_token_idx + 1) % box.tokens.size();
                m_cursor.current_def_page = 0;
            } else if (!box.tokens.empty() && box.tokens[0].definitions.size() > 1) {
                m_cursor.current_def_page = (m_cursor.current_def_page + 1) % box.tokens[0].definitions.size();
            }
        }
    }

    // Button Y: export to Anki
    if (keysDown & HidNpadButton_Y) {
        triggerAnkiMining();
    }

    // Button X: re-scan screen
    if (keysDown & HidNpadButton_X) {
        triggerScan();
    }
#else
    (void)keysDown; (void)keysHeld;
#endif
}

void OverlayGui::render() {
#ifdef __SWITCH__
    auto renderer = tsl::gfx::Renderer::get();

    // 1. Draw detected bounding boxes
    for (size_t i = 0; i < m_ocrData.boxes.size(); ++i) {
        const auto& b = m_ocrData.boxes[i];
        bool isHovered = ((int)i == m_cursor.active_box_idx);

        // Semi-transparent box background
        tsl::Color boxBg = isHovered ? tsl::Color{0, 150, 255, 90} : tsl::Color{20, 20, 40, 60};
        tsl::Color boxBorder = isHovered ? tsl::Color{0, 220, 255, 220} : tsl::Color{180, 180, 200, 140};

        renderer->drawRect(b.rect.x, b.rect.y, b.rect.w, b.rect.h, boxBg);
        // Border outline
        renderer->drawRect(b.rect.x, b.rect.y, b.rect.w, 2, boxBorder);
        renderer->drawRect(b.rect.x, b.rect.y + b.rect.h - 2, b.rect.w, 2, boxBorder);
        renderer->drawRect(b.rect.x, b.rect.y, 2, b.rect.h, boxBorder);
        renderer->drawRect(b.rect.x + b.rect.w - 2, b.rect.y, 2, b.rect.h, boxBorder);
    }

    // 2. Draw definition card if a token is hovered
    if (m_state == OverlayState::READY && m_cursor.active_box_idx >= 0 && m_cursor.active_box_idx < (int)m_ocrData.boxes.size()) {
        const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
        if (!box.tokens.empty() && m_cursor.active_token_idx >= 0 && m_cursor.active_token_idx < (int)box.tokens.size()) {
            const auto& token = box.tokens[m_cursor.active_token_idx];

            // Card placement: top if cursor is low, bottom if cursor is high
            int cardY = (m_cursor.y > 360) ? 30 : 500;
            int cardX = 140;
            int cardW = 1000;
            int cardH = 180;

            // Glassmorphic dark card
            renderer->drawRect(cardX, cardY, cardW, cardH, tsl::Color{12, 14, 24, 230});
            renderer->drawRect(cardX, cardY, cardW, 2, tsl::Color{0, 180, 255, 200});

            // Card Header: Word & Furigana
            std::string header = token.word;
            if (!token.reading.empty() && token.reading != token.word) {
                header += " [" + token.reading + "]";
            }
            if (!token.pitch.empty()) {
                header += "  Pitch: " + token.pitch[0];
            }
            renderer->drawString(header.c_str(), false, cardX + 24, cardY + 20, 24, tsl::Color{255, 255, 255, 255});

            // Definitions
            int defY = cardY + 60;
            if (!token.definitions.empty()) {
                size_t page = m_cursor.current_def_page % token.definitions.size();
                std::string defText = std::to_string(page + 1) + ". " + token.definitions[page];
                renderer->drawString(defText.c_str(), false, cardX + 24, defY, 18, tsl::Color{220, 230, 245, 255});
            }

            // Context sentence preview
            std::string ctx = "Phrase: " + box.text;
            renderer->drawString(ctx.c_str(), false, cardX + 24, cardY + 115, 15, tsl::Color{160, 175, 190, 200});

            // Card footer hint
            std::string navHint = "[A/R] Mot/Def suivant (" + std::to_string(m_cursor.active_token_idx + 1) + "/" + std::to_string(box.tokens.size()) + ")  |  [Y] Exporter Anki";
            renderer->drawString(navHint.c_str(), false, cardX + 24, cardY + 148, 14, tsl::Color{0, 200, 255, 220});
        }
    }

    // 3. Draw Virtual Cursor (Circle / Crosshair)
    tsl::Color curCol = (m_cursor.active_box_idx >= 0) ? tsl::Color{0, 255, 220, 240} : tsl::Color{255, 255, 255, 200};
    renderer->drawRect(m_cursor.x - 6, m_cursor.y - 1, 13, 3, curCol);
    renderer->drawRect(m_cursor.x - 1, m_cursor.y - 6, 3, 13, curCol);

    // 4. Status notifications & bottom controls bar
    if (!m_statusMessage.empty()) {
        renderer->drawRect(340, 20, 600, 44, tsl::Color{20, 25, 40, 240});
        renderer->drawString(m_statusMessage.c_str(), false, 360, 32, 17, tsl::Color{255, 220, 100, 255});
    }

    // Persistent footer
    renderer->drawRect(0, 690, 1280, 30, tsl::Color{10, 12, 18, 220});
    renderer->drawString("[Stick L] Curseur  |  [A/R] Def  |  [Y] Anki  |  [X] Re-scan  |  [B] Quitter", false, 340, 696, 14, tsl::Color{200, 200, 200, 200});
#endif
}

} // namespace switch_ocr
