// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/packet_send_queue.h"

namespace gazeora {

// The UDP protocol: each frame cut into GZF1 datagrams (frame_packet.h), sent to every
// player that said hello lately. Players say where they are by saying hello, so the frames go
// back the way the hello came: through the NAT of an Android emulator too. A hello counts once
// it carries the cookie the PC's challenge gave that address. A lost datagram costs its frame;
// the player asks for a key frame and skips to it.
class UdpFrameOutput : public FrameOutput {
public:
    // Players streamed to at once; hellos from more are ignored.
    static constexpr size_t kMaxPlayers = 8;
    // Addresses challenged and not yet answering; past this, the oldest are forgotten (they get
    // a new challenge with their next hello).
    static constexpr size_t kMaxChallenges = 256;

    explicit UdpFrameOutput(StreamLog& log) : m_log(log) {}
    ~UdpFrameOutput() override { Stop(); }

    bool Start(in_addr address, uint16_t port, std::wstring& error) override;
    void Stop() override;
    uint16_t Port() const override { return m_port; }
    int Viewers() const override { return m_viewers; }
    bool TakeKeyFrameRequest() override { return m_keyFrameWanted.exchange(false); }
    void Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
              uint64_t now) override;
    std::wstring Url(const std::wstring& host) const override;
    bool Watching(const in_addr& address) override;

private:
    struct Player {
        sockaddr_in address;
        uint32_t cookie = 0;
        uint64_t lastHeardMs = 0;
        float loss = 0.0f;  // from its last hello
        bool lossReported = true;
    };

    void ReceiveLoop();
    void Hear(const sockaddr_in& from, const FrameHeader& header, uint64_t now);
    // The cookie for the address with `key`, made and kept if it has none. Under m_playersMutex.
    uint32_t CookieFor(uint64_t key);
    void SendChallenge(const sockaddr_in& to, uint32_t cookie);

    StreamLog& m_log;
    net::Socket m_socket;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<int> m_viewers{0};
    std::atomic<bool> m_keyFrameWanted{false};
    std::thread m_receiveThread;

    std::mutex m_playersMutex;
    std::map<uint64_t, Player> m_players;  // by address and port
    // Cookies sent to addresses that aren't players yet, by address and port, oldest first.
    std::deque<std::pair<uint64_t, uint32_t>> m_challenges;
    bool m_fullLogged = false;

    // Sends the datagrams paced, off the encode thread; while started only.
    std::unique_ptr<PacketSendQueue> m_sendQueue;
    // The encode thread's.
    uint32_t m_frameNumber = 0;
    std::vector<std::string> m_datagrams;
};

}  // namespace gazeora
