#include "gazeora/host/stream/rtsp_connection.h"

#include <algorithm>

namespace gazeora {

bool RtspConnection::Send(const std::string& data) {
    std::lock_guard<std::mutex> lock(m_outMutex);
    m_out += data;
    return FlushLocked();
}

bool RtspConnection::Flush() {
    std::lock_guard<std::mutex> lock(m_outMutex);
    return FlushLocked();
}

bool RtspConnection::HasPending() {
    std::lock_guard<std::mutex> lock(m_outMutex);
    return !m_out.empty();
}

size_t RtspConnection::Backlog() {
    std::lock_guard<std::mutex> lock(m_outMutex);
    return m_out.size();
}

void RtspConnection::QueueInterleaved(uint8_t channel, const std::vector<std::string>& packets,
                                      const std::string* report) {
    std::lock_guard<std::mutex> lock(m_outMutex);
    for (const std::string& packet : packets) {
        AppendFrame(channel, packet);
    }
    if (report) {
        AppendFrame(uint8_t(channel + 1), *report);
    }
    FlushLocked();
}

void RtspConnection::AppendFrame(uint8_t channel, const std::string& data) {
    char header[4] = {'$', char(channel), char(data.size() >> 8), char(data.size())};
    m_out.append(header, 4);
    m_out += data;
}

bool RtspConnection::FlushLocked() {
    size_t sent = 0;
    bool ok = true;
    while (sent < m_out.size()) {
        int n = send(m_socket.Get(), m_out.data() + sent,
                     int(std::min<size_t>(m_out.size() - sent, 1 << 20)), 0);
        if (n > 0) {
            sent += size_t(n);
        } else {
            ok = n < 0 && WSAGetLastError() == WSAEWOULDBLOCK;  // full for now
            break;
        }
    }
    m_out.erase(0, sent);
    return ok;
}

}  // namespace gazeora
