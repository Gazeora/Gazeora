// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/packet_send_queue.h"

#include <algorithm>
#include <utility>

// Missing from older MinGW headers.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace gazeora {

namespace {

constexpr LONGLONG kHundredNsPerSecond = 10000000;
constexpr LONGLONG kMicrosecondsPerSecond = 1000000;

}  // namespace

PacketSendQueue::PacketSendQueue()
    : m_timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_ALL_ACCESS)) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    m_qpcFrequency = frequency.QuadPart;
    m_thread = std::thread([this] { Run(); });
}

PacketSendQueue::~PacketSendQueue() {
    Stop();
    if (m_timer) {
        CloseHandle(m_timer);
    }
}

void PacketSendQueue::Enqueue(SOCKET socket, std::vector<sockaddr_in> targets,
                              const std::vector<std::string>& packets, const PacingPlan& plan) {
    if (targets.empty() || packets.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.push_back(Job{socket, std::move(targets), packets, plan});
        m_pacedWaiting += plan.Bursts() > 1;
    }
    m_wake.notify_one();
}

void PacketSendQueue::Flush() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_stopping || (m_jobs.empty() && !m_sending); });
}

void PacketSendQueue::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        m_jobs.clear();
        m_pacedWaiting = 0;
    }
    m_wake.notify_all();
    m_idle.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void PacketSendQueue::Run() {
    while (true) {
        Job job{INVALID_SOCKET, {}, {}, PacingPlan::AtOnce(0)};
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_sending = false;
            m_idle.notify_all();
            m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
            if (m_stopping) {
                return;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
            m_pacedWaiting -= job.plan.Bursts() > 1;
            m_sending = true;
        }
        Send(job);
    }
}

void PacketSendQueue::Send(const Job& job) {
    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);
    const LONGLONG interval =
        PacingPlan::kBurstIntervalUs * m_qpcFrequency / kMicrosecondsPerSecond;
    const size_t perBurst = job.plan.PacketsPerBurst();
    size_t burst = 0;
    for (size_t first = 0; first < job.packets.size(); first += perBurst, ++burst) {
        // Behind with another frame waiting: no more waits for this one.
        if (burst > 0 && !Backlogged()) {
            WaitUntil(start.QuadPart + LONGLONG(burst) * interval);
        }
        size_t end = std::min(first + perBurst, job.packets.size());
        for (const sockaddr_in& target : job.targets) {
            for (size_t i = first; i < end; ++i) {
                // A lost or refused packet only costs that frame.
                sendto(job.socket, job.packets[i].data(), int(job.packets[i].size()), 0,
                       reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            }
        }
    }
}

bool PacketSendQueue::Backlogged() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pacedWaiting > 0 || m_stopping;
}

void PacketSendQueue::WaitUntil(LONGLONG due) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    LONGLONG remaining = due - now.QuadPart;
    if (remaining <= 0) {
        return;
    }
    LARGE_INTEGER relative;
    relative.QuadPart = -(remaining * kHundredNsPerSecond / m_qpcFrequency);  // negative: relative
    if (m_timer && SetWaitableTimer(m_timer, &relative, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(m_timer, INFINITE);
    } else {
        Sleep(1);
    }
}

}  // namespace gazeora
