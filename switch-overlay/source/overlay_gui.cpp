#include "overlay_gui.hpp"
#include "config.hpp"
#include "discovery.hpp"
#include "http_client.hpp"
#include "screen_capture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#include <tesla.hpp>
#endif

namespace switch_ocr {

#ifdef __SWITCH__
void OverlayGui::scanThreadFunc(void* arg) {
    auto* self = static_cast<OverlayGui*>(arg);
    if (self) {
        self->runScanPipeline();
    }
}
#endif

OverlayGui::OverlayGui() {
    m_cursor.x = 224;
    m_cursor.y = 360;
}

OverlayGui::~OverlayGui() {
#ifdef __SWITCH__
    if (m_scanThreadStarted) {
        threadWaitForExit(&m_scanThread);
        threadClose(&m_scanThread);
        m_scanThreadStarted = false;
    }
#endif
}

void OverlayGui::init() {
    m_config = ConfigManager::load();
    m_state = OverlayState::READY;
    m_statusMessage = "Switch OCR Pret\nAppuyez sur [X] pour analyser";
}

void OverlayGui::triggerScan() {
    if (m_isScanning) return;

    m_isScanning = true;
    m_state = OverlayState::SCANNING;
    m_statusMessage = "Capture & analyse OCR en cours...";

#ifdef __SWITCH__
    if (m_scanThreadStarted) {
        threadWaitForExit(&m_scanThread);
        threadClose(&m_scanThread);
        m_scanThreadStarted = false;
    }

    Result rc = threadCreate(&m_scanThread, scanThreadFunc, this, nullptr, 0x10000, 0x2C, -2);
    if (R_SUCCEEDED(rc)) {
        m_scanThreadStarted = true;
        threadStart(&m_scanThread);
    } else {
        m_isScanning = false;
        m_state = OverlayState::ERROR;
        m_statusMessage = "Erreur creation thread.";
    }
#else
    runScanPipeline();
#endif
}

void OverlayGui::runScanPipeline() {
    // 1. UDP Auto-discovery if enabled
    if (m_config.auto_discovery) {
        std::string foundIp;
        int foundPort = 8766;
        if (DiscoveryClient::discoverServer(m_config.discovery_port, foundIp, foundPort, 1000)) {
            m_config.server_ip = foundIp;
            m_config.server_port = foundPort;
        }
    }

    // 2. Prevent dead-connect timeout if discovery failed and IP is unconfigured default
    if (m_config.server_ip == "192.168.1.100") {
        m_state = OverlayState::ERROR;
        m_statusMessage = "Serveur non detecte (Wi-Fi):\nDefinissez server_ip dans config.ini";
        m_isScanning = false;
        return;
    }

    // 3. Capture screen to JPEG
    std::vector<uint8_t> jpeg;
    if (!ScreenCapture::captureJpeg(jpeg)) {
        m_state = OverlayState::ERROR;
        m_statusMessage = "Erreur capture ecran.";
        m_isScanning = false;
        return;
    }

    // 4. HTTP OCR request
    OCRResponse resp = HttpClient::performOcr(m_config.server_ip, m_config.server_port, jpeg.data(), jpeg.size(), 6);
    if (!resp.success) {
        m_state = OverlayState::ERROR;
        m_statusMessage = resp.error_message.empty() ? ("Serveur injoignable:\n" + m_config.server_ip + ":" + std::to_string(m_config.server_port)) : resp.error_message;
        m_isScanning = false;
        return;
    }

    // 5. OCR success: populate results
    m_ocrData = std::move(resp);
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

    m_isScanning = false;
}

void OverlayGui::triggerAnkiMining() {
    if (m_isScanning) return;
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
    m_statusMessage = ok ? "Mot exporte vers Anki !" : "Erreur export Anki.";
    m_notificationTimer = 120; // ~2 seconds
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

void OverlayGui::update() {
#ifdef __SWITCH__
    if (m_scanThreadStarted && !m_isScanning) {
        threadWaitForExit(&m_scanThread);
        threadClose(&m_scanThread);
        m_scanThreadStarted = false;
    }
#endif

    if (m_state == OverlayState::MINING_NOTIFICATION) {
        if (--m_notificationTimer <= 0) {
            m_state = OverlayState::READY;
            m_statusMessage = "";
        }
    }
}

#ifdef __SWITCH__
bool OverlayGui::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    // Touch screen handling
    if (touchPos.x > 0 && touchPos.y > 0) {
        selectTokenAt(touchPos.x, touchPos.y);
    }

    // D-Pad and Analog Stick movement
    int dx = 0;
    int dy = 0;
    int speed = m_config.cursor_speed;

    if (keysHeld & HidNpadButton_Left)  dx -= speed;
    if (keysHeld & HidNpadButton_Right) dx += speed;
    if (keysHeld & HidNpadButton_Up)    dy -= speed;
    if (keysHeld & HidNpadButton_Down)  dy += speed;

    if (std::abs(leftJoyStick.x) > 8000) {
        dx += (leftJoyStick.x > 0 ? speed : -speed);
    }
    if (std::abs(leftJoyStick.y) > 8000) {
        dy += (leftJoyStick.y > 0 ? -speed : speed);
    }

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
        return true;
    }

    // Button Y: export to Anki
    if (keysDown & HidNpadButton_Y) {
        triggerAnkiMining();
        return true;
    }

    // Button X: re-scan screen
    if (keysDown & HidNpadButton_X) {
        if (!m_isScanning) {
            triggerScan();
        }
        return true;
    }

    // Button B: exit overlay
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    return false;
}

void OverlayGui::render(tsl::gfx::Renderer* renderer, s32 frameX, s32 frameY, s32 frameW, s32 frameH) {
    if (!renderer) return;

    // RGBA4444 color definitions (each channel 0x0 - 0xF)
    constexpr tsl::Color colTextWhite = { 0xF, 0xF, 0xF, 0xF };
    constexpr tsl::Color colTextYellow = { 0xF, 0xD, 0x2, 0xF };
    constexpr tsl::Color colTextCyan = { 0x0, 0xD, 0xF, 0xF };
    constexpr tsl::Color colTextGray = { 0xA, 0xA, 0xA, 0xF };
    constexpr tsl::Color colCardBg = { 0x1, 0x1, 0x2, 0xF };
    constexpr tsl::Color colCardBorder = { 0x0, 0x9, 0xF, 0xF };

    // 1. Status header / Notification
    s32 contentStartY = frameY + 45;
    if (!m_statusMessage.empty()) {
        std::vector<std::string> lines;
        std::stringstream ss(m_statusMessage);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) lines.push_back(line);
        }

        s32 bannerH = 14 + (s32)lines.size() * 20;
        renderer->drawRect(frameX, frameY, frameW, bannerH, tsl::gfx::Renderer::a(colCardBg));
        for (size_t i = 0; i < lines.size(); ++i) {
            tsl::Color col = (i == 0) ? colTextYellow : colTextGray;
            float sz = (i == 0) ? 15.0f : 13.0f;
            renderer->drawString(lines[i].c_str(), false, frameX + 10, frameY + 18 + (s32)i * 20, sz, tsl::gfx::Renderer::a(col));
        }
        contentStartY = frameY + bannerH + 8;
    }

    // 2. Display detected content / Active token definition
    if (m_state == OverlayState::READY && m_cursor.active_box_idx >= 0 && m_cursor.active_box_idx < (int)m_ocrData.boxes.size()) {
        const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
        s32 cardY = contentStartY;

        if (!box.tokens.empty() && m_cursor.active_token_idx >= 0 && m_cursor.active_token_idx < (int)box.tokens.size()) {
            const auto& token = box.tokens[m_cursor.active_token_idx];

            // Card background & border
            renderer->drawRect(frameX, cardY, frameW, 280, tsl::gfx::Renderer::a(colCardBg));
            renderer->drawRect(frameX, cardY, frameW, 2, tsl::gfx::Renderer::a(colCardBorder));

            // Word & Reading
            std::string header = token.word;
            if (!token.reading.empty() && token.reading != token.word) {
                header += " [" + token.reading + "]";
            }
            if (!token.pitch.empty()) {
                header += "  P:" + token.pitch[0];
            }
            renderer->drawString(header.c_str(), false, frameX + 10, cardY + 30, 22.0f, tsl::gfx::Renderer::a(colTextWhite));

            // Definitions
            s32 defY = cardY + 65;
            if (!token.definitions.empty()) {
                size_t page = m_cursor.current_def_page % token.definitions.size();
                std::string defLine = std::to_string(page + 1) + ". " + token.definitions[page];
                renderer->drawString(defLine.c_str(), false, frameX + 10, defY, 16.0f, tsl::gfx::Renderer::a(colTextCyan));
            }

            // Context sentence
            std::string ctx = "Texte: " + box.text;
            renderer->drawString(ctx.c_str(), false, frameX + 10, cardY + 200, 14.0f, tsl::gfx::Renderer::a(colTextGray));

            // Token navigation counter
            std::string count = "Mot " + std::to_string(m_cursor.active_token_idx + 1) + "/" + std::to_string(box.tokens.size()) + " (A: Suivant)";
            renderer->drawString(count.c_str(), false, frameX + 10, cardY + 250, 14.0f, tsl::gfx::Renderer::a(colTextYellow));
        } else {
            // Box raw text
            renderer->drawRect(frameX, cardY, frameW, 100, tsl::gfx::Renderer::a(colCardBg));
            std::string raw = "Texte: " + box.text;
            renderer->drawString(raw.c_str(), false, frameX + 10, cardY + 40, 16.0f, tsl::gfx::Renderer::a(colTextWhite));
        }
    } else if (m_state == OverlayState::SCANNING) {
        renderer->drawString("Traitement en cours...", false, frameX + 10, contentStartY + 20, 18.0f, tsl::gfx::Renderer::a(colTextCyan));
    } else if (m_state == OverlayState::READY && m_ocrData.boxes.empty() && m_statusMessage.empty()) {
        renderer->drawString("Aucun texte detecte.", false, frameX + 10, frameY + 80, 18.0f, tsl::gfx::Renderer::a(colTextGray));
    }

    // 3. Bottom controls footer
    s32 footerY = frameY + frameH - 40;
    renderer->drawRect(frameX, footerY, frameW, 40, tsl::gfx::Renderer::a(colCardBg));
    renderer->drawString("\uE0E0 Def  \uE0E3 Anki  \uE0E2 Scan  \uE0E1 Retour", false, frameX + 10, footerY + 26, 15.0f, tsl::gfx::Renderer::a(colTextWhite));
}
#endif

} // namespace switch_ocr
