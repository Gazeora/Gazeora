// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <cstdint>
#include <vector>

#include "gazeora/host/stream/pointer_event.h"

namespace gazeora::app {

// Adapter pattern: the phone's touches as touch-screen input to this window's UI, and nothing
// else on the PC. Works while the window is minimized too, as long as it keeps drawing.
class ImGuiPointerInput {
public:
    // Between the platform backend's NewFrame and ImGui::NewFrame, so the touches come after
    // what the backend read of the real mouse and win over it.
    void Apply(const std::vector<PointerEvent>& events, ImGuiIO& io, uint64_t nowMs);
    // Whether ImGui still has input waiting, touches or the mouse's: keep drawing until not.
    static bool HasQueuedInput();

private:
    void ApplyOne(const PointerEvent& event, ImGuiIO& io);
    void MoveTo(const PointerEvent& event, ImGuiIO& io);
    void FlushWheel(ImGuiIO& io);
    // The backend puts the real cursor back every frame while this window has focus.
    bool HoldsPosition(uint64_t nowMs) const;

    ImVec2 m_position;
    bool m_pressed = false;
    uint64_t m_lastTouchMs = 0;
    int m_wheelVertical = 0;
    int m_wheelHorizontal = 0;
};

}  // namespace gazeora::app
