#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/pointer_event_sink.h"
#include "gazeora/host/stream/stream_log.h"

namespace gazeora {

// The app's touches over the network: GZF1 Pointer datagrams (pointer_message.h) on a UDP
// port of their own, whichever protocol the frames go out on.
class PointerInputReceiver {
public:
    PointerInputReceiver(PointerEventSink& sink, StreamLog& log) : m_sink(sink), m_log(log) {}
    ~PointerInputReceiver() { Stop(); }
    PointerInputReceiver(const PointerInputReceiver&) = delete;
    PointerInputReceiver& operator=(const PointerInputReceiver&) = delete;

    // Listens on `address`:`port` (0: any free port). False with `error` if the port is taken.
    bool Start(in_addr address, uint16_t port, std::wstring& error);
    void Stop();
    uint16_t Port() const { return m_port; }

private:
    void Loop();
    void Hear(const uint8_t* datagram, size_t size);

    PointerEventSink& m_sink;
    StreamLog& m_log;
    net::Socket m_socket;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::thread m_thread;
};

}  // namespace gazeora
