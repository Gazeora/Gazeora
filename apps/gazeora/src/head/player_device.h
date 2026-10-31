// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Which the player is playing the game with: its controller, or the keyboard and mouse.
class PlayerDevice {
public:
    virtual ~PlayerDevice() = default;
    // From any thread.
    virtual bool UsingController() const = 0;
};

}  // namespace gazeora::app
