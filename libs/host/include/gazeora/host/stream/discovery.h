#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/stream_log.h"

namespace gazeora {

// Lets the Android app find the PC without typing an address. The app sends kDiscoveryRequest
// in a UDP datagram to kDiscoveryPort, broadcast on its network (and to 10.0.2.2, the PC as
// an Android emulator sees it); the reply goes back to the sender, one line of JSON:
//   {"service":"gazeora","version":1,"name":"MY-PC","protocol":"rtsp-tcp","port":8554,
//    "path":"/","codec":"h264"}
// `protocol` is a stream_protocol.h id, `port` 0 for USB. The address to connect to is the
// one the reply came from.
constexpr uint16_t kDiscoveryPort = 8556;
constexpr char kDiscoveryRequest[] = "GAZEORA-DISCOVER 1";

class DiscoveryResponder {
public:
    // Builds the reply, when asked (so it's always current).
    using Describe = std::function<std::string()>;

    explicit DiscoveryResponder(StreamLog& log) : m_log(log) {}
    DiscoveryResponder(const DiscoveryResponder&) = delete;
    DiscoveryResponder& operator=(const DiscoveryResponder&) = delete;
    ~DiscoveryResponder() { Stop(); }

    // Answers on `address`:`port` (0: any free port). False with `error` set if the port is
    // taken. Needs Winsock started.
    bool Start(in_addr address, uint16_t port, Describe describe, std::wstring& error);
    void Stop();
    uint16_t Port() const { return m_port; }

private:
    void Loop();

    StreamLog& m_log;
    Describe m_describe;
    net::Socket m_socket;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::thread m_thread;
};

// `text` as a JSON string, quotes included.
std::string JsonString(const std::string& text);

}  // namespace gazeora
