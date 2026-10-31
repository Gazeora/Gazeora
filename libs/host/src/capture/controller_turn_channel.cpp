// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/capture/controller_turn_channel.h"

#include <cmath>

namespace gazeora {

namespace {

constexpr double kMicro = 1e6;
constexpr double kPermille = 1000.0;

LONGLONG QpcNow() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

LONGLONG QpcPerSecond() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

}  // namespace

void ControllerTurnChannel::Attach(DWORD pid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (pid == m_pid && (pid == 0 || m_shared)) {
        return;
    }
    Detach();
    m_pid = pid;
    if (pid == 0) {
        return;
    }
    m_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SharedMemoryName(pid).c_str());
    auto* shared = m_mapping ? static_cast<SharedHeader*>(MapViewOfFile(
                                   m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)))
                             : nullptr;
    if (!shared || shared->magic != kSharedMagic || shared->version != kSharedVersion) {
        if (shared) {
            UnmapViewOfFile(shared);
        }
        Detach();
        return;
    }
    m_shared = shared;
    WriteStick();
}

bool ControllerTurnChannel::Attached() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shared != nullptr;
}

void ControllerTurnChannel::AddTurn(double yawDegrees, double pitchDegrees) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shared) {
        InterlockedExchangeAdd64(&m_shared->headYawMicro,
                                 LONG64(std::llround(yawDegrees * kMicro)));
        InterlockedExchangeAdd64(&m_shared->headPitchMicro,
                                 LONG64(std::llround(pitchDegrees * kMicro)));
    }
}

void ControllerTurnChannel::SetStick(float degreesPerSecond, float deadZone) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_degreesPerSecond = degreesPerSecond;
    m_deadZone = deadZone;
    WriteStick();
}

ControllerTurnChannel::Reads ControllerTurnChannel::Read() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Reads reads;
    if (!m_shared) {
        return reads;
    }
    const LONGLONG read = m_shared->controllerReadQpc;
    reads.recent = read != 0 && QpcNow() - read < QpcPerSecond();
    reads.activeQpc = m_shared->controllerActiveQpc;
    reads.paths = m_shared->controllerPaths;
    return reads;
}

void ControllerTurnChannel::Detach() {
    if (m_shared) {
        UnmapViewOfFile(m_shared);
        m_shared = nullptr;
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
    }
    m_pid = 0;
}

void ControllerTurnChannel::WriteStick() {
    if (m_shared) {
        InterlockedExchange(&m_shared->stickDegreesPerSecond,
                            LONG(std::lround(m_degreesPerSecond)));
        InterlockedExchange(&m_shared->stickDeadZonePermille,
                            LONG(std::lround(m_deadZone * kPermille)));
    }
}

}  // namespace gazeora
