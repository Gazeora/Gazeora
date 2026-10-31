// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "ui/gamepad_scroll.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace gazeora::app {

namespace {

// Pixels a second with the stick pushed all the way, at 100% scaling.
constexpr float kFullSpeed = 1800.0f;
// The first frame after the UI sat idle can come late; don't jump by all that time.
constexpr float kMaxFrameSeconds = 1.0f / 20.0f;

float Push(ImGuiKey key) { return ImGui::GetKeyData(key)->AnalogValue; }

}  // namespace

void GamepadScroll::Apply() const {
    float push = Push(ImGuiKey_GamepadRStickDown) - Push(ImGuiKey_GamepadRStickUp);
    if (push == 0.0f) {
        return;
    }
    float seconds = std::min(ImGui::GetIO().DeltaTime, kMaxFrameSeconds);
    float speed = push * std::fabs(push) * kFullSpeed * m_scale;
    ImGui::SetScrollY(ImGui::GetScrollY() + speed * seconds);
}

}  // namespace gazeora::app
