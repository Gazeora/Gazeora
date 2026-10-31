// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

#include "gazeora/host/stream/head_angles.h"
#include "gazeora/host/stream/head_pose_sink.h"
#include "head/camera_driver.h"
#include "head/game_focus.h"

namespace gazeora::app {

// Head tracking: each head pose from the phone, as the turn since the one before, to the game's
// camera, while it's switched on and the game is in front. Only turns count, so wherever the
// head points when it starts steering, or again after a pause, is where the camera already
// looks.
class HeadLook final : public HeadPoseSink {
public:
    using Clock = std::function<uint64_t()>;  // milliseconds

    // `clock` defaults to GetTickCount64.
    HeadLook(CameraDriver& camera, const GameFocus& focus, Clock clock = {});

    // From any thread.
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool Enabled() const { return m_enabled; }
    // Poses have come from the phone in the last second.
    bool Receiving() const;
    bool GameInFront() const { return m_focus.GameInFront(); }

    // From the stream's threads.
    void OnHeadPose(const HeadPose& pose) override;

private:
    struct Last {
        uint32_t sample;
        HeadAngles angles;
        uint64_t atMs;
    };

    bool Steering() const { return m_enabled && m_focus.GameInFront(); }
    // How many samples `pose` comes after the last one: 0 or less if it arrived late, far
    // less if the phone started counting again.
    int32_t SamplesAhead(const HeadPose& pose) const;
    void Steer(const HeadPose& pose, uint64_t nowMs);
    void TurnFrom(const HeadAngles& earlier, const HeadAngles& now);
    void Stop();

    CameraDriver& m_camera;
    const GameFocus& m_focus;
    Clock m_clock;
    std::atomic<bool> m_enabled{false};
    std::atomic<uint64_t> m_lastPoseMs{0};
    std::mutex m_mutex;
    std::optional<Last> m_last;  // with m_mutex
};

}  // namespace gazeora::app
