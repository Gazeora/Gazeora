// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/input/models/gamepad_state.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Mapper pattern: a GamepadState and the plugin interface's GazeoraGamepadStateV3, both ways.
// Their buttons and axes are numbered alike.
class PluginGamepadState final {
public:
    PluginGamepadState() = delete;

    static GazeoraGamepadStateV3 ToPlugin(const input::GamepadState& state);
    // Bits past the last button are ignored, and sticks clamped to -1..1.
    static input::GamepadState FromPlugin(const GazeoraGamepadStateV3& state);
};

}  // namespace gazeora
