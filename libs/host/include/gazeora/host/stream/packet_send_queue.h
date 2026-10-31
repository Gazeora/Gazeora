// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gazeora/host/stream/pacing_plan.h"

namespace gazeora {

// Active Object pattern: sends UDP packets from a thread of its own, spread as each frame's
// PacingPlan says, so the encode thread hands a frame over and gets on with the next instead
// of waiting out the pacing. Bursts keep to a schedule from the frame's first one, so a timer
// that wakes late doesn't add up over the frame; with another paced frame waiting, the rest of
// the current one goes out without waits (packets sent at once, such as an RTCP report, don't
// count). Packets go out in the order they were queued.
// Destroy it (or Stop) before closing the sockets it sends on.
class PacketSendQueue {
public:
    PacketSendQueue();
    PacketSendQueue(const PacketSendQueue&) = delete;
    PacketSendQueue& operator=(const PacketSendQueue&) = delete;
    ~PacketSendQueue();

    // Queues `packets` to every one of `targets`, on `socket`. Returns at once.
    void Enqueue(SOCKET socket, std::vector<sockaddr_in> targets,
                 const std::vector<std::string>& packets, const PacingPlan& plan);
    // Waits until everything queued has gone out.
    void Flush();
    // Drops what's still queued and ends the thread.
    void Stop();

private:
    struct Job {
        SOCKET socket;
        std::vector<sockaddr_in> targets;
        std::vector<std::string> packets;
        PacingPlan plan;
    };

    void Run();
    void Send(const Job& job);
    bool Backlogged();
    // Sleeps until the QPC time `due`; at once if it's past.
    void WaitUntil(LONGLONG due);

    HANDLE m_timer = nullptr;  // high resolution: Sleep(1) often takes 15 ms
    LONGLONG m_qpcFrequency = 0;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<Job> m_jobs;
    size_t m_pacedWaiting = 0;  // of m_jobs, those in more than one burst
    bool m_sending = false;
    bool m_stopping = false;
    std::thread m_thread;
};

}  // namespace gazeora
