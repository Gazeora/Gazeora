// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gamepad/gamepad_control.h"
#include "plugin_ui/gazeora_ui_values.h"

namespace gazeora::app {

// The game controller session's values for plugin panels, as the Game Controller plugin's
// card shows them: gazeora.gamepad.status (its line), .tone (that line's tone id), .failed
// ("1" when controllers can't be read) and the action gazeora.gamepad.retry.
class GamepadUiValues final {
public:
    GamepadUiValues() = delete;

    // `control` must outlive `values`.
    static void AddTo(GazeoraUiValues& values, GamepadControl& control);
};

}  // namespace gazeora::app
