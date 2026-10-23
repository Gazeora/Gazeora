#include "gazeora/host/stream/udp_frame_output.h"

#include <ws2tcpip.h>

#include <algorithm>

// Missing from older MinGW headers.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace gazeora {

namespace {

constexpr int kSendBufferBytes = 8 << 20;

uint64_t Key(const sockaddr_in& address) {
    return uint64_t(address.sin_addr.s_addr) << 16 | address.sin_port;
}

}  // namespace

bool UdpFrameOutput::Start(in_addr address, uint16_t port, std::wstring& error) {
    uint16_t bound = 0;
    m_socket = net::BindUdp(address, port, bound);
    if (!m_socket) {
        error = L"cannot open a UDP port (error " + std::to_wstring(WSAGetLastError()) + L")";
        return false;
    }
    m_socket.SetSendBuffer(kSendBufferBytes);
    // Without this, a datagram to a player that has gone makes the next read fail.
    BOOL reportResets = FALSE;
    DWORD returned = 0;
    WSAIoctl(m_socket.Get(), SIO_UDP_CONNRESET, &reportResets, sizeof(reportResets), nullptr, 0,
             &returned, nullptr, nullptr);
    m_stopping = false;
    m_port = bound;
    m_log.Write("UDP frames on port %u", unsigned(bound));
    m_receiveThread = std::thread([this] { ReceiveLoop(); });
    return true;
}

void UdpFrameOutput::Stop() {
    m_stopping = true;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    m_socket.Reset();
    m_port = 0;
    std::lock_guard<std::mutex> lock(m_playersMutex);
    m_players.clear();
    m_viewers = 0;
}

std::wstring UdpFrameOutput::Url(const std::wstring& host) const {
    return L"udp://" + host + L":" + std::to_wstring(m_port);
}

void UdpFrameOutput::ReceiveLoop() {
    uint8_t datagram[2048];
    while (!m_stopping) {
        // Poll, so Stop doesn't have to close the socket out from under recvfrom.
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_socket.Get(), &readable);
        timeval timeout = {0, 250 * 1000};
        uint64_t now = GetTickCount64();
        if (select(0, &readable, nullptr, nullptr, &timeout) == 1) {
            sockaddr_in from = {};
            int fromLength = sizeof(from);
            int got = recvfrom(m_socket.Get(), reinterpret_cast<char*>(datagram), sizeof(datagram),
                               0, reinterpret_cast<sockaddr*>(&from), &fromLength);
            FrameHeader header;
            now = GetTickCount64();
            if (got > 0 && header.Read(datagram, size_t(got))) {
                Hear(from, header, now);
            }
        }
        // Players that stopped saying hello have gone.
        std::lock_guard<std::mutex> lock(m_playersMutex);
        for (auto it = m_players.begin(); it != m_players.end();) {
            if (now - it->second.lastHeardMs > kPlayerTimeoutMs) {
                m_log.Write("%s stopped saying hello: dropped",
                            net::AddressText(it->second.address).c_str());
                it = m_players.erase(it);
            } else {
                ++it;
            }
        }
        m_viewers = int(m_players.size());
    }
}

void UdpFrameOutput::Hear(const sockaddr_in& from, const FrameHeader& header, uint64_t now) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    auto it = m_players.find(Key(from));
    switch (header.type) {
        case FrameMessage::Hello:
            if (it == m_players.end()) {
                m_log.Write("%s said hello: streaming to it", net::AddressText(from).c_str());
                it = m_players.emplace(Key(from), Player{from}).first;
                m_keyFrameWanted = true;  // so it doesn't wait for one
            }
            it->second.lastHeardMs = now;
            it->second.loss = std::min(header.lossPerMille, uint16_t(1000)) / 1000.0f;
            it->second.lossReported = false;
            break;
        case FrameMessage::KeyFrameRequest:
            m_keyFrameWanted = true;
            break;
        case FrameMessage::Bye:
            if (it != m_players.end()) {
                m_log.Write("%s said bye", net::AddressText(from).c_str());
                m_players.erase(it);
            }
            break;
        default:
            break;
    }
    m_viewers = int(m_players.size());
}

void UdpFrameOutput::Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
                          uint64_t now) {
    std::vector<sockaddr_in> targets;
    {
        std::lock_guard<std::mutex> lock(m_playersMutex);
        for (auto& entry : m_players) {
            Player& player = entry.second;
            targets.push_back(player.address);
            if (!player.lossReported) {
                bitrate.Report(entry.first, player.loss, now);
                player.lossReported = true;
            }
        }
    }
    FrameHeader header;
    header.flags = uint8_t((frame.keyFrame ? kFrameFlagKey : 0) |
                           (frame.codec == VideoCodec::H265 ? kFrameFlagH265 : 0));
    header.width = uint16_t(frame.width);
    header.height = uint16_t(frame.height);
    header.frameNumber = m_frameNumber++;
    header.timestamp = frame.timestamp;
    ChunkFrame(header, frame.annexB, m_datagrams);
    if (targets.empty()) {
        return;
    }
    if (!m_pacer) {
        m_pacer = std::make_unique<PacketPacer>();  // on the thread that uses it
    }
    // Spread like RTP: a key frame sent all at once overflows Wi-Fi buffers.
    size_t perBurst = PacketPacer::PacketsPerBurst(m_datagrams.size());
    for (size_t first = 0; first < m_datagrams.size(); first += perBurst) {
        if (first > 0) {
            m_pacer->WaitForNextBurst();
        }
        size_t end = std::min(first + perBurst, m_datagrams.size());
        for (const sockaddr_in& target : targets) {
            for (size_t i = first; i < end; ++i) {
                sendto(m_socket.Get(), m_datagrams[i].data(), int(m_datagrams[i].size()), 0,
                       reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            }
        }
    }
}

}  // namespace gazeora
