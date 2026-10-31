// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/input/interfaces/gamepad_listener.h"
#include "gazeora/input/platform/sdl_gamepad_driver_factory.h"
#include "gazeora/input/services/gamepad_session.h"

namespace gazeora::input {

// Facade pattern: game controllers wired to the real system (SDL). The launcher's
// composition root for them; `listener` hears what they do and must outlive this.
class GamepadServices final {
public:
    explicit GamepadServices(IGamepadListener& listener) : m_session(m_drivers, listener) {}

    GamepadServices(const GamepadServices&) = delete;
    GamepadServices& operator=(const GamepadServices&) = delete;

    GamepadSession& Session() { return m_session; }

private:
    // Declared before m_session, which holds a reference to it.
    SdlGamepadDriverFactory m_drivers;
    GamepadSession m_session;
};

}  // namespace gazeora::input
