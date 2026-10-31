// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_event_queue.h"

namespace gazeora::app {

namespace {

// States beyond this while the UI isn't taking them (minimized, or it hung) push out the
// oldest: the latest is what the controller does now, so it's never the one dropped.
constexpr size_t kMaxQueued = 256;

}  // namespace

void GamepadEventQueue::OnGamepadState(const input::GamepadState& state) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_states.size() >= kMaxQueued) {
            m_states.pop_front();
        }
        m_states.push_back(state);
    }
    m_wake();
}

std::vector<input::GamepadState> GamepadEventQueue::Take() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<input::GamepadState> taken(m_states.begin(), m_states.end());
    m_states.clear();
    return taken;
}

}  // namespace gazeora::app
