#include "http_client.hpp"

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <vector>

namespace switch_ocr {

struct HttpResponse {
    bool ok = false;
    int status_code = 0;
    std::string body;
    std::string error;
};

static HttpResponse postRequest(
    const std::string& ip,
    int port,
    const std::string& path,
    const std::string& contentType,
    const uint8_t* bodyData,
    size_t bodySize,
    int timeoutSec
) {
    HttpResponse resp;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        resp.error = "Erreur socket (" + std::string(strerror(errno)) + ")";
        return resp;
    }

    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    // Disable Nagle's algorithm to eliminate 40-200ms latency buffering over Wi-Fi
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

    // Expand socket buffers for fast frame transfer
    int bufSize = 65536;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        close(sock);
        resp.error = "Format IP invalide:\n" + ip;
        return resp;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        int err = errno;
        close(sock);
        resp.error = "Echec connexion:\n" + ip + ":" + std::to_string(port) + " (" + strerror(err) + ")";
        return resp;
    }

    std::ostringstream header;
    header << "POST " << path << " HTTP/1.1\r\n"
           << "Host: " << ip << ":" << port << "\r\n"
           << "Content-Type: " << contentType << "\r\n"
           << "Content-Length: " << bodySize << "\r\n"
           << "Connection: close\r\n\r\n";

    std::string headerStr = header.str();
    ssize_t hSent = send(sock, headerStr.c_str(), headerStr.length(), 0);
    if (hSent < (ssize_t)headerStr.length()) {
        int err = errno;
        close(sock);
        resp.error = "Envoi header echec (" + std::string(strerror(err)) + ")";
        return resp;
    }

    // Send payload
    size_t totalSent = 0;
    while (totalSent < bodySize) {
        ssize_t sent = send(sock, (const char*)bodyData + totalSent, bodySize - totalSent, 0);
        if (sent <= 0) {
            int err = errno;
            close(sock);
            resp.error = "Envoi body echec:\n" + std::to_string(totalSent) + "/" + std::to_string(bodySize) + " (" + strerror(err) + ")";
            return resp;
        }
        totalSent += sent;
    }

    // Read response
    std::string response;
    response.reserve(16384);
    char buffer[8192];
    ssize_t bytesRead = 0;
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
    }
    close(sock);

    if (response.empty()) {
        resp.error = "Timeout reponse OCR:\n" + ip + ":" + std::to_string(port);
        return resp;
    }

    // Check HTTP status code
    auto firstLineEnd = response.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        std::string statusLine = response.substr(0, firstLineEnd);
        auto space1 = statusLine.find(' ');
        if (space1 != std::string::npos) {
            auto space2 = statusLine.find(' ', space1 + 1);
            std::string codeStr = (space2 != std::string::npos) 
                ? statusLine.substr(space1 + 1, space2 - space1 - 1)
                : statusLine.substr(space1 + 1);
            resp.status_code = std::atoi(codeStr.c_str());
        }
    }

    if (resp.status_code != 200 && resp.status_code != 0) {
        resp.error = "Erreur HTTP " + std::to_string(resp.status_code) + ":\n" + ip + ":" + std::to_string(port);
        return resp;
    }

    // Extract body after \r\n\r\n
    auto bodyPos = response.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        resp.body = response.substr(bodyPos + 4);
        resp.ok = true;
        return resp;
    }

    resp.error = "Reponse incomplete (" + ip + ")";
    return resp;
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
        if (!item.empty()) {
            result.push_back(std::atoi(item.c_str()));
        }
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
    HttpResponse httpResp = postRequest(serverIp, serverPort, "/api/ocr", "image/jpeg", jpegData, jpegSize, timeoutSec);
    if (!httpResp.ok) {
        resp.success = false;
        resp.error_message = httpResp.error.empty() ? "Serveur injoignable." : httpResp.error;
        return resp;
    }

    const std::string& json = httpResp.body;
    if (json.find("\"status\":\"ok\"") == std::string::npos && json.find("\"status\": \"ok\"") == std::string::npos) {
        resp.success = false;
        resp.error_message = "Erreur reponse OCR (" + serverIp + ")";
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
    HttpResponse httpResp = postRequest(
        serverIp,
        serverPort,
        "/api/anki",
        "application/json",
        (const uint8_t*)payload.data(),
        payload.length(),
        timeoutSec
    );

    if (!httpResp.ok) return false;
    return httpResp.body.find("\"success\":true") != std::string::npos || 
           httpResp.body.find("\"success\": true") != std::string::npos;
}

} // namespace switch_ocr
