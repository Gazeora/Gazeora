// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <optional>

#include "gamepad/gamepad_control.h"
#include "ui/gamepad_status_line.h"

namespace gazeora::app {

// The game controller status line, made again only when the session says something new or a
// game starts or stops: a card asks for it several times each frame, and making it copies the
// session's snapshot under its lock.
class GamepadStatusCache {
public:
    explicit GamepadStatusCache(const GamepadControl& control) : m_control(control) {}

    const GamepadStatusLine& Line();
    bool Failed();

private:
    void Refresh();

    const GamepadControl& m_control;
    std::optional<GamepadStatusLine> m_line;
    bool m_failed = false;
    uint64_t m_version = 0;
    bool m_paused = false;
};

}  // namespace gazeora::app
