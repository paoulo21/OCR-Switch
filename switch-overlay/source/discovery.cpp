#include "discovery.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace switch_ocr {

bool DiscoveryClient::discoverServer(int discoveryPort, std::string& outIp, int& outPort, int timeoutMs) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    int broadcastEnable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    struct sockaddr_in clientAddr;
    std::memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddr.sin_port = 0;
    bind(sock, (struct sockaddr*)&clientAddr, sizeof(clientAddr));

    struct sockaddr_in broadcastAddr;
    std::memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(discoveryPort);
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    const char* pingMsg = "DISCOVER_SWITCH_OCR";
    
    // Poll loop with retries every 750ms up to timeoutMs
    int elapsed = 0;
    constexpr int INTERVAL_MS = 750;
    
    while (elapsed < timeoutMs) {
        sendto(sock, pingMsg, std::strlen(pingMsg), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        
        int waitTime = std::min(INTERVAL_MS, timeoutMs - elapsed);
        int ret = poll(&pfd, 1, waitTime);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buffer[256];
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);

            ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&senderAddr, &senderLen);
            if (received > 0) {
                buffer[received] = '\0';
                std::string msg(buffer);
                // Format: "SWITCH_OCR_SERVER:8766"
                const std::string prefix = "SWITCH_OCR_SERVER:";
                auto pos = msg.find(prefix);
                if (pos != std::string::npos) {
                    outIp = inet_ntoa(senderAddr.sin_addr);
                    std::string portStr = msg.substr(pos + prefix.length());
                    outPort = std::atoi(portStr.c_str());
                    if (outPort <= 0) outPort = 8766;
                    close(sock);
                    return true;
                }
            }
        }
        elapsed += waitTime;
    }

    close(sock);
    return false;
}

} // namespace switch_ocr
