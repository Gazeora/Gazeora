#include "gazeora/host/stream/discovery.h"

#include <cstdio>
#include <cstring>

namespace gazeora {

bool DiscoveryResponder::Start(in_addr address, uint16_t port, Describe describe,
                               std::wstring& error) {
    m_socket.Reset(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (!m_socket) {
        error = L"socket failed (error " + std::to_wstring(WSAGetLastError()) + L")";
        return false;
    }
    // Another program on this port would get the requests instead.
    BOOL exclusive = TRUE;
    setsockopt(m_socket.Get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<char*>(&exclusive), sizeof(exclusive));
    sockaddr_in bound = {};
    bound.sin_family = AF_INET;
    bound.sin_addr = address;
    bound.sin_port = htons(port);
    int length = sizeof(bound);
    if (bind(m_socket.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) != 0 ||
        getsockname(m_socket.Get(), reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        error = L"UDP port " + std::to_wstring(port) + L" for discovery is taken (error " +
                std::to_wstring(WSAGetLastError()) + L")";
        m_socket.Reset();
        return false;
    }
    m_describe = std::move(describe);
    m_stopping = false;
    m_port = ntohs(bound.sin_port);
    m_thread = std::thread([this] { Loop(); });
    return true;
}

void DiscoveryResponder::Stop() {
    m_stopping = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_socket.Reset();
    m_port = 0;
}

void DiscoveryResponder::Loop() {
    char request[256];
    while (!m_stopping) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_socket.Get(), &readable);
        timeval timeout = {0, 250 * 1000};
        if (select(0, &readable, nullptr, nullptr, &timeout) != 1) {
            continue;
        }
        sockaddr_in from = {};
        int fromLength = sizeof(from);
        int got = recvfrom(m_socket.Get(), request, sizeof(request), 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLength);
        const size_t size = sizeof(kDiscoveryRequest) - 1;
        if (got < int(size) || memcmp(request, kDiscoveryRequest, size) != 0) {
            continue;  // someone else's datagram
        }
        std::string reply = m_describe();
        sendto(m_socket.Get(), reply.data(), int(reply.size()), 0,
               reinterpret_cast<const sockaddr*>(&from), sizeof(from));
    }
}

std::string JsonString(const std::string& text) {
    std::string json = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            json += '\\';
            json += char(c);
        } else if (c < 0x20) {
            char escaped[8];
            snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            json += escaped;
        } else {
            json += char(c);
        }
    }
    return json + "\"";
}

}  // namespace gazeora
