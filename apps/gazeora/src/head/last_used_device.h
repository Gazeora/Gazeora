// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/capture/controller_turn_channel.h"
#include "head/keyboard_mouse_watch.h"
#include "head/player_device.h"

namespace gazeora::app {

// The player is on the controller when the game reads one and it was used after the keyboard
// and mouse were, as games switch their prompts and controls to whichever was used last.
class LastUsedDevice final : public PlayerDevice {
public:
    LastUsedDevice(const ControllerTurnChannel& controller, const KeyboardMouseWatch& keyboardMouse)
        : m_controller(controller), m_keyboardMouse(keyboardMouse) {}

    bool UsingController() const override;

private:
    const ControllerTurnChannel& m_controller;
    const KeyboardMouseWatch& m_keyboardMouse;
};

}  // namespace gazeora::app
