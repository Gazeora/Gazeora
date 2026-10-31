// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/input/services/gamepad_session.h"
#include "settings/controls_settings.h"

namespace gazeora::app {

// The game controller switch, and when controllers are read: while it's on and no game
// launched from here runs. A running game has them to itself, untouched, and they're read
// again as soon as it's closed.
class GamepadControl {
public:
    GamepadControl(input::GamepadSession& session, const ControlsSettings& store);

    // Starts reading, if the saved switch is on.
    void Resume();
    // Saves the switch and starts or stops reading to match.
    void SetEnabled(bool enabled);
    void SetGameRunning(bool running);
    // Reads them again, e.g. after an error.
    void Restart() { Apply(); }

    bool Enabled() const { return m_enabled; }
    bool Paused() const { return m_enabled && m_gameRunning; }
    // A controller is connected and being read.
    bool Connected() const;
    input::GamepadSnapshot Snapshot() const { return m_session.Snapshot(); }

private:
    void Apply();
    bool Reading() const { return m_enabled && !m_gameRunning; }

    input::GamepadSession& m_session;
    const ControlsSettings& m_store;
    bool m_enabled;
    bool m_gameRunning = false;
};

}  // namespace gazeora::app
