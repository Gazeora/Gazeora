// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>

#include <cstdint>

#include "gazeora/host/stream/frame_packet.h"

namespace gazeora {

// A Gazeora app on loopback, over UDP: says hello, answers the challenge, takes frames.
class UdpPlayer {
public:
    static constexpr DWORD kReceiveTimeoutMs = 3000;

    explicit UdpPlayer(uint16_t port) : m_socket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) {
        DWORD timeout = kReceiveTimeoutMs;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout),
                   sizeof(timeout));
        m_server.sin_family = AF_INET;
        m_server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        m_server.sin_port = htons(port);
    }
    ~UdpPlayer() { closesocket(m_socket); }

    bool Join() {
        Say(0);
        FrameHeader challenge;
        if (!Receive(challenge) || challenge.type != FrameMessage::Challenge) {
            return false;
        }
        Say(challenge.timestamp);
        return true;
    }

    // The next datagram's header; false on a timeout.
    bool Receive(FrameHeader& header) {
        char datagram[kMaxFrameDatagram];
        const int got = recv(m_socket, datagram, sizeof(datagram), 0);
        return got > 0 && header.Read(reinterpret_cast<uint8_t*>(datagram), size_t(got));
    }

private:
    void Say(uint32_t cookie) {
        FrameHeader header;
        header.type = FrameMessage::Hello;
        header.timestamp = cookie;
        uint8_t raw[kFrameHeaderSize];
        header.Write(raw);
        sendto(m_socket, reinterpret_cast<char*>(raw), sizeof(raw), 0,
               reinterpret_cast<sockaddr*>(&m_server), sizeof(m_server));
    }

    SOCKET m_socket;
    sockaddr_in m_server = {};
};

}  // namespace gazeora
