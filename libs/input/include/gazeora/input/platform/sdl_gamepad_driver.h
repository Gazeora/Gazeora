// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <map>

#include "gazeora/input/interfaces/gamepad_driver.h"

struct SDL_Gamepad;

namespace gazeora::input {

// Adapter pattern: game controllers through SDL, which knows the layouts of nearly every
// make (XInput, DirectInput, Windows.Gaming.Input and HID ones such as the DualSense).
// Leaves controllers as games expect them: no enhanced DualSense reports, no lights.
class SdlGamepadDriver final : public IGamepadDriver {
public:
    // Throws InputError if SDL can't read game controllers.
    SdlGamepadDriver();
    ~SdlGamepadDriver() override;

    SdlGamepadDriver(const SdlGamepadDriver&) = delete;
    SdlGamepadDriver& operator=(const SdlGamepadDriver&) = delete;

    void Update() override;
    std::vector<GamepadInfo> Connected() const override;
    GamepadState State() const override;

private:
    using GamepadId = uint32_t;  // SDL_JoystickID

    void Open(GamepadId id);
    void Close(GamepadId id);
    static GamepadState Read(SDL_Gamepad* gamepad);

    std::map<GamepadId, SDL_Gamepad*> m_gamepads;
};

}  // namespace gazeora::input
