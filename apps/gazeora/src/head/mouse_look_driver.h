// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>

#include "head/camera_driver.h"
#include "head/mouse_mover.h"

namespace gazeora::app {

// Turns the camera the way mouse-look games take it: as mouse moves, a fixed number of counts
// per degree. That number depends on the game and its sensitivity setting, so it's the user's
// to set. Parts of a count are carried over, so slow turns aren't lost to rounding.
class MouseLookDriver final : public CameraDriver {
public:
    static constexpr float kMinCountsPerDegree = 1.0f;
    static constexpr float kMaxCountsPerDegree = 100.0f;
    static constexpr float kDefaultCountsPerDegree = 20.0f;

    explicit MouseLookDriver(MouseMover& mouse) : m_mouse(mouse) {}

    // From any thread; clamped to kMin..kMaxCountsPerDegree.
    void SetCountsPerDegree(float counts);
    float CountsPerDegree() const { return m_countsPerDegree; }

    void Turn(double yawDegrees, double pitchDegrees) override;
    void Release() override;

private:
    // Whole counts out of `wanted`, the rest kept in `carried`.
    static int TakeCounts(double wanted, double& carried);

    MouseMover& m_mouse;
    std::atomic<float> m_countsPerDegree{kDefaultCountsPerDegree};
    double m_carriedX = 0.0;
    double m_carriedY = 0.0;
};

}  // namespace gazeora::app
