// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/udp_frame_output.h"

#include <ws2tcpip.h>
// After winsock2.h (by way of the header), which brings in windows.h.
#include <bcrypt.h>

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
    m_sendQueue = std::make_unique<PacketSendQueue>();
    m_log.Write("UDP frames on port %u", unsigned(bound));
    m_receiveThread = std::thread([this] { ReceiveLoop(); });
    return true;
}

void UdpFrameOutput::Stop() {
    m_stopping = true;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    m_sendQueue.reset();  // before the socket it sends on goes
    m_socket.Reset();
    m_port = 0;
    std::lock_guard<std::mutex> lock(m_playersMutex);
    m_players.clear();
    m_challenges.clear();
    m_fullLogged = false;
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
                m_fullLogged = false;
            } else {
                ++it;
            }
        }
        m_viewers = int(m_players.size());
    }
}

bool UdpFrameOutput::Watching(const in_addr& address) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return std::any_of(m_players.begin(), m_players.end(), [&](const auto& entry) {
        return entry.second.address.sin_addr.s_addr == address.s_addr;
    });
}

uint32_t UdpFrameOutput::CookieFor(uint64_t key) {
    for (const auto& challenge : m_challenges) {
        if (challenge.first == key) {
            return challenge.second;
        }
    }
    // From the system's cryptographic generator: one a player could predict from the cookies
    // it was given could be forged for any address.
    uint32_t cookie = 0;
    while (cookie == 0) {  // 0 is what a player that hasn't been challenged sends
        BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&cookie), sizeof(cookie),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    }
    if (m_challenges.size() >= kMaxChallenges) {
        m_challenges.pop_front();
    }
    m_challenges.emplace_back(key, cookie);
    return cookie;
}

void UdpFrameOutput::SendChallenge(const sockaddr_in& to, uint32_t cookie) {
    FrameHeader header;
    header.type = FrameMessage::Challenge;
    header.timestamp = cookie;
    uint8_t raw[kFrameHeaderSize];
    header.Write(raw);
    sendto(m_socket.Get(), reinterpret_cast<const char*>(raw), sizeof(raw), 0,
           reinterpret_cast<const sockaddr*>(&to), sizeof(to));
}

void UdpFrameOutput::Hear(const sockaddr_in& from, const FrameHeader& header, uint64_t now) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    const uint64_t key = Key(from);
    auto it = m_players.find(key);
    if (it == m_players.end() && header.type != FrameMessage::Hello) {
        return;  // only a player's count
    }
    // Only with the cookie this address was sent: then it comes from where it says.
    const uint32_t cookie = it != m_players.end() ? it->second.cookie : CookieFor(key);
    if (header.timestamp != cookie) {
        if (header.type == FrameMessage::Hello) {
            SendChallenge(from, cookie);  // the size of the hello: nothing gained by forging it
        }
        return;
    }
    switch (header.type) {
        case FrameMessage::Hello:
            if (it == m_players.end()) {
                if (m_players.size() >= kMaxPlayers) {
                    if (!m_fullLogged) {
                        m_log.Write("%s said hello, but %u players are watching already",
                                    net::AddressText(from).c_str(), unsigned(m_players.size()));
                        m_fullLogged = true;
                    }
                    break;
                }
                m_log.Write("%s said hello: streaming to it", net::AddressText(from).c_str());
                it = m_players.emplace(key, Player{from, cookie}).first;
                m_challenges.erase(
                    std::remove_if(m_challenges.begin(), m_challenges.end(),
                                   [&](const auto& challenge) { return challenge.first == key; }),
                    m_challenges.end());
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
            m_log.Write("%s said bye", net::AddressText(from).c_str());
            m_players.erase(it);
            m_fullLogged = false;
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
    if (targets.empty() || !m_sendQueue) {
        return;
    }
    // Spread like RTP: a key frame sent all at once overflows Wi-Fi buffers.
    PacingPlan plan =
        PacingPlan::For(m_datagrams.size(), frame.annexB.size(), frame.bitrate, frame.fps);
    m_sendQueue->Enqueue(m_socket.Get(), std::move(targets), m_datagrams, plan);
}

}  // namespace gazeora
