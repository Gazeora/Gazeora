// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "gazeora/host/stream/head_pose_sink.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/pointer_event_sink.h"
#include "gazeora/host/stream/stream_log.h"

namespace gazeora {

// The app's input over the network: GZF1 Pointer datagrams (pointer_message.h) and HeadPose
// ones (head_pose_message.h) on a UDP port of their own, whichever protocol the frames go out
// on. Only from the addresses `admits` takes: anything else on the network could click around
// Gazeora, or turn a game's camera, otherwise.
class RemoteInputReceiver {
public:
    using Admits = std::function<bool(const in_addr& address)>;

    RemoteInputReceiver(PointerEventSink& pointer, HeadPoseSink& head, StreamLog& log,
                        Admits admits)
        : m_pointer(pointer), m_head(head), m_log(log), m_admits(std::move(admits)) {}
    ~RemoteInputReceiver() { Stop(); }
    RemoteInputReceiver(const RemoteInputReceiver&) = delete;
    RemoteInputReceiver& operator=(const RemoteInputReceiver&) = delete;

    // Listens on `address`:`port` (0: any free port). False with `error` if the port is taken.
    bool Start(in_addr address, uint16_t port, std::wstring& error);
    void Stop();
    uint16_t Port() const { return m_port; }

private:
    void Loop();
    void Hear(const sockaddr_in& from, const uint8_t* datagram, size_t size);
    void Dispatch(const uint8_t* datagram, size_t size);

    PointerEventSink& m_pointer;
    HeadPoseSink& m_head;
    StreamLog& m_log;
    Admits m_admits;
    net::Socket m_socket;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::thread m_thread;
};

}  // namespace gazeora
