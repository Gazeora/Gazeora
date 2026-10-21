#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gazeora/host/stream/net.h"

namespace gazeora {

// One player's RTSP connection (non-blocking). Everything sent on it queues here: RTSP
// replies from the connection's own thread and interleaved RTP from the encoder, so a slow
// player can't hold either up. The queue is flushed as far as the socket takes it without
// blocking; the connection's thread flushes the rest when the socket has room.
class RtspConnection {
public:
    RtspConnection(net::Socket socket, std::string peer)
        : m_socket(std::move(socket)), m_peer(std::move(peer)) {}
    RtspConnection(const RtspConnection&) = delete;
    RtspConnection& operator=(const RtspConnection&) = delete;

    SOCKET Socket() const { return m_socket.Get(); }
    const std::string& Peer() const { return m_peer; }  // "address:port", for the log

    // Queues `data` and sends what the socket takes. False if the connection is broken.
    bool Send(const std::string& data);
    // Sends more of what's queued. False if the connection is broken.
    bool Flush();
    bool HasPending();
    size_t Backlog();  // bytes still queued

    // Queues RTP packets interleaved on `channel` (RFC 2326 10.12), and `report` on the
    // channel after it if there is one. A broken connection is noticed by its own thread.
    void QueueInterleaved(uint8_t channel, const std::vector<std::string>& packets,
                          const std::string* report);

    std::thread thread;
    std::atomic<bool> done{false};
    std::atomic<bool> interleaved{false};  // RTP goes out on it

private:
    bool FlushLocked();
    void AppendFrame(uint8_t channel, const std::string& data);

    net::Socket m_socket;
    std::string m_peer;
    std::mutex m_outMutex;
    std::string m_out;
};

}  // namespace gazeora
