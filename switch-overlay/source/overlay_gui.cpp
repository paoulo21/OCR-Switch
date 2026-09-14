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

OverlayGui::OverlayGui() {
    m_cursor.x = 224;
    m_cursor.y = 360;
}

OverlayGui::~OverlayGui() {}

void OverlayGui::init() {
    m_config = ConfigManager::load();
    m_state = OverlayState::READY;
    m_statusMessage = "Capture & analyse OCR...";
    // Auto-scan immediately upon opening overlay
    triggerScan();
}

void OverlayGui::triggerScan() {
    if (m_state == OverlayState::SCANNING) return;
    m_state = OverlayState::SCANNING;
    m_statusMessage = "Capture & analyse OCR...";

    // 1. Quick UDP auto-discovery (500 ms) if enabled
    if (m_config.auto_discovery) {
        std::string foundIp;
        int foundPort = 8766;
        if (DiscoveryClient::discoverServer(m_config.discovery_port, foundIp, foundPort, 500)) {
            m_config.server_ip = foundIp;
            m_config.server_port = foundPort;
        }
    }

    // 2. Prevent connecting to default placeholder IP if discovery failed
    if (m_config.server_ip == "192.168.1.100") {
        m_state = OverlayState::ERROR;
        m_statusMessage = "Serveur non detecte (Wi-Fi):\nDefinissez server_ip dans config.ini";
        return;
    }

    // 3. Screen capture (~5ms)
    std::vector<uint8_t> jpeg;
    if (!ScreenCapture::captureJpeg(jpeg)) {
        m_state = OverlayState::ERROR;
        m_statusMessage = "Erreur capture ecran.";
        return;
    }

    // 4. HTTP OCR request (6s timeout)
    OCRResponse resp = HttpClient::performOcr(m_config.server_ip, m_config.server_port, jpeg.data(), jpeg.size(), 6);
    if (!resp.success) {
        m_state = OverlayState::ERROR;
        m_statusMessage = resp.error_message.empty() ? ("Serveur injoignable:\n" + m_config.server_ip + ":" + std::to_string(m_config.server_port)) : resp.error_message;
        return;
    }

    // 5. Success: populate results
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
}

void OverlayGui::triggerAnkiMining() {
    if (m_state == OverlayState::SCANNING) return;
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
    if (m_state == OverlayState::MINING_NOTIFICATION) {
        if (--m_notificationTimer <= 0) {
            m_state = OverlayState::READY;
            m_statusMessage = "";
        }
    }
}

#ifdef __SWITCH__
bool OverlayGui::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (m_state == OverlayState::READY && !m_ocrData.boxes.empty()) {
        int numBoxes = (int)m_ocrData.boxes.size();

        // 1. D-Pad Up / Down (or Left Stick Up / Down): cycle sentences
        bool goUp = (keysDown & HidNpadButton_Up) || (leftJoyStick.y > 18000 && !(keysHeld & HidNpadButton_Up));
        bool goDown = (keysDown & HidNpadButton_Down) || (leftJoyStick.y < -18000 && !(keysHeld & HidNpadButton_Down));

        if (goUp) {
            m_cursor.active_box_idx = (m_cursor.active_box_idx - 1 + numBoxes) % numBoxes;
            const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
            m_cursor.active_token_idx = box.tokens.empty() ? -1 : 0;
            m_cursor.current_def_page = 0;
            return true;
        }
        if (goDown) {
            m_cursor.active_box_idx = (m_cursor.active_box_idx + 1) % numBoxes;
            const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
            m_cursor.active_token_idx = box.tokens.empty() ? -1 : 0;
            m_cursor.current_def_page = 0;
            return true;
        }

        // 2. D-Pad Left / Right (or Left Stick Left / Right): cycle words within active sentence
        if (m_cursor.active_box_idx >= 0 && m_cursor.active_box_idx < numBoxes) {
            const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
            if (!box.tokens.empty()) {
                int numTokens = (int)box.tokens.size();
                if ((keysDown & HidNpadButton_Right) || (leftJoyStick.x > 18000)) {
                    m_cursor.active_token_idx = (m_cursor.active_token_idx + 1) % numTokens;
                    m_cursor.current_def_page = 0;
                    return true;
                }
                if ((keysDown & HidNpadButton_Left) || (leftJoyStick.x < -18000)) {
                    m_cursor.active_token_idx = (m_cursor.active_token_idx - 1 + numTokens) % numTokens;
                    m_cursor.current_def_page = 0;
                    return true;
                }
            }
        }
    }

    // Button A or R: cycle definitions of active word
    if ((keysDown & HidNpadButton_A) || (keysDown & HidNpadButton_R)) {
        if (m_cursor.active_box_idx >= 0 && m_cursor.active_box_idx < (int)m_ocrData.boxes.size()) {
            const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];
            if (m_cursor.active_token_idx >= 0 && m_cursor.active_token_idx < (int)box.tokens.size()) {
                const auto& token = box.tokens[m_cursor.active_token_idx];
                if (token.definitions.size() > 1) {
                    m_cursor.current_def_page = (m_cursor.current_def_page + 1) % token.definitions.size();
                } else if (box.tokens.size() > 1) {
                    m_cursor.active_token_idx = (m_cursor.active_token_idx + 1) % box.tokens.size();
                    m_cursor.current_def_page = 0;
                }
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
        triggerScan();
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
    constexpr tsl::Color colHighlight = { 0x2, 0x2, 0x4, 0xF };

    // 1. Status header / Notification
    s32 contentStartY = frameY + 45;
    if (!m_statusMessage.empty()) {
        auto nl = m_statusMessage.find('\n');
        if (nl != std::string::npos) {
            std::string line1 = m_statusMessage.substr(0, nl);
            std::string line2 = m_statusMessage.substr(nl + 1);
            renderer->drawRect(frameX, frameY, frameW, 54, tsl::gfx::Renderer::a(colCardBg));
            renderer->drawString(line1.c_str(), false, frameX + 10, frameY + 20, 15.0f, tsl::gfx::Renderer::a(colTextYellow));
            renderer->drawString(line2.c_str(), false, frameX + 10, frameY + 40, 13.0f, tsl::gfx::Renderer::a(colTextGray));
            contentStartY = frameY + 62;
        } else {
            renderer->drawRect(frameX, frameY, frameW, 36, tsl::gfx::Renderer::a(colCardBg));
            renderer->drawString(m_statusMessage.c_str(), false, frameX + 10, frameY + 22, 15.0f, tsl::gfx::Renderer::a(colTextYellow));
            contentStartY = frameY + 44;
        }
    }

    // 2. Display detected content / Active token definition
    if (m_state == OverlayState::READY && !m_ocrData.boxes.empty()) {
        int numBoxes = (int)m_ocrData.boxes.size();
        if (m_cursor.active_box_idx < 0) m_cursor.active_box_idx = 0;
        if (m_cursor.active_box_idx >= numBoxes) m_cursor.active_box_idx = numBoxes - 1;

        const auto& box = m_ocrData.boxes[m_cursor.active_box_idx];

        // Phrase index header
        std::string phraseBadge = "Phrase " + std::to_string(m_cursor.active_box_idx + 1) + "/" + std::to_string(numBoxes);
        if (numBoxes > 1) {
            phraseBadge += "  (\u25B2/\u25BC changer)";
        }
        renderer->drawString(phraseBadge.c_str(), false, frameX + 12, contentStartY + 14, 15.0f, tsl::gfx::Renderer::a(colTextYellow));

        // Full detected sentence box
        s32 sentBoxY = contentStartY + 24;
        renderer->drawRect(frameX + 8, sentBoxY, frameW - 16, 52, tsl::gfx::Renderer::a(colHighlight));
        renderer->drawRect(frameX + 8, sentBoxY, frameW - 16, 1, tsl::gfx::Renderer::a(colCardBorder));
        renderer->drawString(box.text.c_str(), false, frameX + 14, sentBoxY + 28, 17.0f, tsl::gfx::Renderer::a(colTextWhite));

        // Word definition card
        s32 cardY = sentBoxY + 60;
        renderer->drawRect(frameX + 8, cardY, frameW - 16, 180, tsl::gfx::Renderer::a(colCardBg));
        renderer->drawRect(frameX + 8, cardY, frameW - 16, 2, tsl::gfx::Renderer::a(colCardBorder));

        if (!box.tokens.empty() && m_cursor.active_token_idx >= 0 && m_cursor.active_token_idx < (int)box.tokens.size()) {
            const auto& token = box.tokens[m_cursor.active_token_idx];

            // Word & Reading header
            std::string header = token.word;
            if (!token.reading.empty() && token.reading != token.word) {
                header += " [" + token.reading + "]";
            }
            if (!token.pitch.empty()) {
                header += "  P:" + token.pitch[0];
            }
            renderer->drawString(header.c_str(), false, frameX + 16, cardY + 28, 21.0f, tsl::gfx::Renderer::a(colTextWhite));

            // Definitions
            s32 defY = cardY + 60;
            if (!token.definitions.empty()) {
                size_t page = m_cursor.current_def_page % token.definitions.size();
                std::string defLine = std::to_string(page + 1) + ". " + token.definitions[page];
                renderer->drawString(defLine.c_str(), false, frameX + 16, defY, 15.0f, tsl::gfx::Renderer::a(colTextCyan));
            } else {
                renderer->drawString("(Aucune definition trouvee)", false, frameX + 16, defY, 14.0f, tsl::gfx::Renderer::a(colTextGray));
            }

            // Word navigation counter
            std::string count = "Mot " + std::to_string(m_cursor.active_token_idx + 1) + "/" + std::to_string(box.tokens.size()) + "  (\u25C4/\u25BA changer, A: def)";
            renderer->drawString(count.c_str(), false, frameX + 16, cardY + 150, 13.0f, tsl::gfx::Renderer::a(colTextYellow));
        } else {
            renderer->drawString("(Aucun mot japonais identifie)", false, frameX + 16, cardY + 40, 15.0f, tsl::gfx::Renderer::a(colTextGray));
        }

        // 3. List of ALL detected sentences on screen (so the user never gets lost)
        if (numBoxes > 1) {
            s32 listY = cardY + 195;
            renderer->drawString("--- Toutes les phrases detectees ---", false, frameX + 12, listY, 13.0f, tsl::gfx::Renderer::a(colTextGray));
            s32 itemY = listY + 20;

            for (int i = 0; i < numBoxes && i < 5; ++i) {
                bool isActive = (i == m_cursor.active_box_idx);
                std::string line = (isActive ? "\u25B6 " : "  ") + std::to_string(i + 1) + ". " + m_ocrData.boxes[i].text;
                if (line.size() > 36) {
                    line = line.substr(0, 33) + "...";
                }
                tsl::Color col = isActive ? colTextYellow : colTextGray;
                renderer->drawString(line.c_str(), false, frameX + 12, itemY, 14.0f, tsl::gfx::Renderer::a(col));
                itemY += 22;
            }
        }
    } else if (m_state == OverlayState::SCANNING) {
        renderer->drawString("Traitement en cours...", false, frameX + 10, contentStartY + 20, 18.0f, tsl::gfx::Renderer::a(colTextCyan));
    } else if (m_state == OverlayState::READY && m_ocrData.boxes.empty() && m_statusMessage.empty()) {
        renderer->drawString("Aucun texte japonais detecte.", false, frameX + 10, frameY + 80, 18.0f, tsl::gfx::Renderer::a(colTextGray));
        renderer->drawString("Appuyez sur [X] pour rescanner.", false, frameX + 10, frameY + 110, 14.0f, tsl::gfx::Renderer::a(colTextCyan));
    }

    // 4. Bottom controls footer
    s32 footerY = frameY + frameH - 40;
    renderer->drawRect(frameX, footerY, frameW, 40, tsl::gfx::Renderer::a(colCardBg));
    renderer->drawString("\u25B2\u25BC Phrase  \u25C4\u25BA Mot  \uE0E0 Def  \uE0E3 Anki  \uE0E2 Scan", false, frameX + 10, footerY + 26, 14.0f, tsl::gfx::Renderer::a(colTextWhite));
}
#endif

} // namespace switch_ocr
