#include "network/http_client.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace switch_ocr {

static std::string postRequest(
    const std::string& ip,
    int port,
    const std::string& path,
    const std::string& contentType,
    const uint8_t* bodyData,
    size_t bodySize,
    int timeoutSec
) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        return "";
    }

    std::ostringstream header;
    header << "POST " << path << " HTTP/1.1\r\n"
           << "Host: " << ip << ":" << port << "\r\n"
           << "Content-Type: " << contentType << "\r\n"
           << "Content-Length: " << bodySize << "\r\n"
           << "Connection: close\r\n\r\n";

    std::string headerStr = header.str();
    send(sock, headerStr.c_str(), headerStr.length(), 0);

    // Send payload
    size_t totalSent = 0;
    while (totalSent < bodySize) {
        ssize_t sent = send(sock, (const char*)bodyData + totalSent, bodySize - totalSent, 0);
        if (sent <= 0) break;
        totalSent += sent;
    }

    // Read response
    std::string response;
    char buffer[4096];
    ssize_t bytesRead = 0;
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
    }
    close(sock);

    // Extract body after \r\n\r\n
    auto bodyPos = response.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        return response.substr(bodyPos + 4);
    }
    return "";
}

// Lightweight manual JSON token extractor
static std::string extractJsonString(const std::string& json, const std::string& key, size_t startPos = 0) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle, startPos);
    if (pos == std::string::npos) return "";

    auto q1 = json.find('"', pos + needle.length());
    if (q1 == std::string::npos) return "";
    auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";

    return json.substr(q1 + 1, q2 - q1 - 1);
}

static std::vector<int> extractJsonIntArray(const std::string& json, const std::string& key, size_t startPos = 0) {
    std::vector<int> result;
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle, startPos);
    if (pos == std::string::npos) return result;

    auto b1 = json.find('[', pos);
    auto b2 = json.find(']', b1);
    if (b1 == std::string::npos || b2 == std::string::npos) return result;

    std::string sub = json.substr(b1 + 1, b2 - b1 - 1);
    std::stringstream ss(sub);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            result.push_back(std::stoi(item));
        } catch (...) {}
    }
    return result;
}

OCRResponse HttpClient::performOcr(
    const std::string& serverIp,
    int serverPort,
    const uint8_t* jpegData,
    size_t jpegSize,
    int timeoutSec
) {
    OCRResponse resp;
    std::string json = postRequest(serverIp, serverPort, "/api/ocr", "image/jpeg", jpegData, jpegSize, timeoutSec);
    if (json.empty()) {
        resp.success = false;
        resp.error_message = "Connexion au serveur impossible.";
        return resp;
    }

    if (json.find("\"status\":\"ok\"") == std::string::npos && json.find("\"status\": \"ok\"") == std::string::npos) {
        resp.success = false;
        resp.error_message = "Erreur serveur OCR.";
        return resp;
    }

    // Parse boxes
    size_t curPos = 0;
    while ((curPos = json.find("\"box\":", curPos)) != std::string::npos) {
        DetectedBox box;
        auto boxCoords = extractJsonIntArray(json, "box", curPos);
        if (boxCoords.size() >= 4) {
            box.rect.x = boxCoords[0];
            box.rect.y = boxCoords[1];
            box.rect.w = boxCoords[2];
            box.rect.h = boxCoords[3];
        }

        box.text = extractJsonString(json, "text", curPos);

        // Find tokens array for this box
        auto tokensPos = json.find("\"tokens\":", curPos);
        auto nextBoxPos = json.find("\"box\":", curPos + 6);
        if (tokensPos != std::string::npos && (nextBoxPos == std::string::npos || tokensPos < nextBoxPos)) {
            size_t tokPos = tokensPos;
            size_t endTok = (nextBoxPos != std::string::npos) ? nextBoxPos : json.length();

            while ((tokPos = json.find("\"word\":", tokPos)) != std::string::npos && tokPos < endTok) {
                TokenMatch tm;
                tm.word = extractJsonString(json, "word", tokPos);
                tm.reading = extractJsonString(json, "reading", tokPos);

                // Extract definitions array
                auto defPos = json.find("\"definitions\":", tokPos);
                if (defPos != std::string::npos && defPos < endTok) {
                    auto d1 = json.find('[', defPos);
                    auto d2 = json.find(']', d1);
                    if (d1 != std::string::npos && d2 != std::string::npos) {
                        std::string defSub = json.substr(d1 + 1, d2 - d1 - 1);
                        size_t q = 0;
                        while ((q = defSub.find('"', q)) != std::string::npos) {
                            auto qNext = defSub.find('"', q + 1);
                            if (qNext == std::string::npos) break;
                            tm.definitions.push_back(defSub.substr(q + 1, qNext - q - 1));
                            q = qNext + 1;
                        }
                    }
                }

                auto range = extractJsonIntArray(json, "char_range", tokPos);
                if (range.size() >= 2) {
                    tm.char_start = range[0];
                    tm.char_end = range[1];
                }

                box.tokens.push_back(tm);
                tokPos += 8;
            }
        }

        resp.boxes.push_back(box);
        curPos += 6;
    }

    resp.success = true;
    return resp;
}

bool HttpClient::exportAnki(
    const std::string& serverIp,
    int serverPort,
    const std::string& word,
    const std::string& reading,
    const std::vector<std::string>& definitions,
    const std::string& sentence,
    int timeoutSec
) {
    std::ostringstream json;
    json << "{"
         << "\"word\":\"" << word << "\","
         << "\"reading\":\"" << reading << "\","
         << "\"sentence\":\"" << sentence << "\","
         << "\"definitions\":[";

    for (size_t i = 0; i < definitions.size(); ++i) {
        if (i > 0) json << ",";
        json << "\"" << definitions[i] << "\"";
    }
    json << "]}";

    std::string payload = json.str();
    std::string res = postRequest(
        serverIp,
        serverPort,
        "/api/anki",
        "application/json",
        (const uint8_t*)payload.data(),
        payload.length(),
        timeoutSec
    );

    return res.find("\"success\":true") != std::string::npos || res.find("\"success\": true") != std::string::npos;
}

} // namespace switch_ocr
