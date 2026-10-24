#include "gazeora/host/stream/pointer_input_receiver.h"

#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/pointer_message.h"

namespace gazeora {

namespace {

constexpr long kPollUs = 250 * 1000;
// Pointer messages are a header and a few bytes; anything longer is not one.
constexpr size_t kMaxDatagram = 256;

}  // namespace

bool PointerInputReceiver::Start(in_addr address, uint16_t port, std::wstring& error) {
    m_socket.Reset(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    sockaddr_in bound = {};
    bound.sin_family = AF_INET;
    bound.sin_addr = address;
    bound.sin_port = htons(port);
    int length = sizeof(bound);
    // The app sends to this exact port: another one would never hear from it.
    if (!m_socket ||
        bind(m_socket.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) != 0 ||
        getsockname(m_socket.Get(), reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        error = L"UDP port " + std::to_wstring(port) + L" for touch input is taken (error " +
                std::to_wstring(WSAGetLastError()) + L")";
        m_socket.Reset();
        return false;
    }
    m_stopping = false;
    m_port = ntohs(bound.sin_port);
    m_log.Write("touch input on UDP port %u", unsigned(m_port));
    m_thread = std::thread([this] { Loop(); });
    return true;
}

void PointerInputReceiver::Stop() {
    m_stopping = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_socket.Reset();
    m_port = 0;
}

void PointerInputReceiver::Loop() {
    uint8_t datagram[kMaxDatagram];
    while (!m_stopping) {
        // Poll, so Stop doesn't have to close the socket out from under recvfrom.
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_socket.Get(), &readable);
        timeval timeout = {0, kPollUs};
        if (select(0, &readable, nullptr, nullptr, &timeout) != 1) {
            continue;
        }
        int got = recv(m_socket.Get(), reinterpret_cast<char*>(datagram), sizeof(datagram), 0);
        if (got > 0) {
            Hear(datagram, size_t(got));
        }
    }
}

void PointerInputReceiver::Hear(const uint8_t* datagram, size_t size) {
    FrameHeader header;
    PointerEvent event;
    if (header.Read(datagram, size) && header.type == FrameMessage::Pointer &&
        PointerMessage::Read(datagram + kFrameHeaderSize, size - kFrameHeaderSize, event)) {
        m_sink.OnPointer(event);
    }
}

}  // namespace gazeora
