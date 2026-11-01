// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/input/interfaces/gamepad_switch.h"
#include "gazeora/input/services/gamepad_session.h"

namespace gazeora::app {

// When game controllers are read: while the Game Controller plugin's switch is on and no game
// launched from here runs. A running game has them to itself, untouched, and they're read
// again as soon as it's closed. The switch is the plugin's, shown on its own card.
class GamepadControl {
public:
    // `enabled` is the plugin's switch; null without the plugin.
    GamepadControl(input::GamepadSession& session,
                   std::shared_ptr<const input::IGamepadSwitch> enabled);

    // Starts or stops reading to match the switch and the game; call every frame.
    void Refresh();
    void SetGameRunning(bool running);
    // Reads them again, e.g. after an error.
    void Restart();

    bool Enabled() const { return m_enabled && m_enabled->Enabled(); }
    // A plugin offers a driver to read controllers with.
    bool Available() const { return m_session.Available(); }
    bool Paused() const { return Enabled() && m_gameRunning; }
    // A controller is connected and being read.
    bool Connected() const;
    input::GamepadSnapshot Snapshot() const { return m_session.Snapshot(); }

private:
    bool Reading() const { return Enabled() && !m_gameRunning; }

    input::GamepadSession& m_session;
    const std::shared_ptr<const input::IGamepadSwitch> m_enabled;
    bool m_gameRunning = false;
    bool m_reading = false;  // what Refresh last applied
};

}  // namespace gazeora::app
