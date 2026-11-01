// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_control.h"

namespace gazeora::app {

GamepadControl::GamepadControl(input::GamepadSession& session,
                               std::shared_ptr<const input::IGamepadSwitch> enabled)
    : m_session(session), m_enabled(std::move(enabled)) {}

void GamepadControl::Refresh() {
    const bool reading = Reading();
    if (reading == m_reading) {
        return;
    }
    m_reading = reading;
    if (reading) {
        m_session.Start();
    } else {
        m_session.Stop();
    }
}

void GamepadControl::SetGameRunning(bool running) {
    m_gameRunning = running;
    Refresh();
}

void GamepadControl::Restart() {
    m_reading = false;
    Refresh();
}

bool GamepadControl::Connected() const {
    return m_session.Running() && !m_session.Snapshot().connected.empty();
}

}  // namespace gazeora::app
