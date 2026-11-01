// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/input/models/gamepad_snapshot.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::app {

// The one line under the game controller switch: which controllers are connected, or why
// none is read. Shown on the Game Controller plugin's card as gazeora.gamepad.status.
class GamepadStatusLine {
public:
    GamepadStatusLine(const input::GamepadSnapshot& snapshot, bool paused);

    const std::string& Text() const { return m_text; }
    ui::UiTone Tone() const { return m_tone; }

private:
    void Describe(const input::GamepadSnapshot& snapshot);
    void DescribeConnected(const input::GamepadSnapshot& snapshot);

    std::string m_text;
    ui::UiTone m_tone;
};

}  // namespace gazeora::app
