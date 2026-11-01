// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/adapters/plugin_gamepad_state.h"

#include <cmath>

namespace gazeora {

namespace {

using input::GamepadAxis;
using input::GamepadButton;

static_assert(input::kGamepadButtonCount == GAZEORA_GAMEPAD_BUTTON_COUNT,
              "the plugin interface numbers every button");
static_assert(int(GamepadButton::DpadRight) == GAZEORA_GAMEPAD_BUTTON_DPAD_RIGHT,
              "buttons are numbered alike");
static_assert(input::kGamepadAxisCount == GAZEORA_GAMEPAD_AXIS_COUNT,
              "the plugin interface numbers every axis");
static_assert(int(GamepadAxis::RightY) == GAZEORA_GAMEPAD_AXIS_RIGHT_Y, "axes are numbered alike");

constexpr uint32_t Bit(int button) { return 1u << unsigned(button); }

}  // namespace

GazeoraGamepadStateV3 PluginGamepadState::ToPlugin(const input::GamepadState& state) {
    GazeoraGamepadStateV3 plugin = {};
    plugin.struct_size = sizeof(plugin);
    for (int button = 0; button < input::kGamepadButtonCount; ++button) {
        if (state.IsDown(GamepadButton(button))) {
            plugin.buttons |= Bit(button);
        }
    }
    for (int axis = 0; axis < input::kGamepadAxisCount; ++axis) {
        plugin.axes[axis] = state.Axis(GamepadAxis(axis));
    }
    return plugin;
}

input::GamepadState PluginGamepadState::FromPlugin(const GazeoraGamepadStateV3& plugin) {
    input::GamepadState state;
    for (int button = 0; button < input::kGamepadButtonCount; ++button) {
        state.SetButton(GamepadButton(button), (plugin.buttons & Bit(button)) != 0);
    }
    for (int axis = 0; axis < input::kGamepadAxisCount; ++axis) {
        // A NaN from a plugin would pass the clamp.
        const float value = plugin.axes[axis];
        state.SetAxis(GamepadAxis(axis), std::isnan(value) ? 0.0f : value);
    }
    return state;
}

}  // namespace gazeora
