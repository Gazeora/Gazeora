// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::input {

// Whether the player wants game controllers to drive Gazeora: the controller plugin's switch.
// Asked on the UI thread.
class IGamepadSwitch {
public:
    virtual ~IGamepadSwitch() = default;

    virtual bool Enabled() const = 0;
};

}  // namespace gazeora::input
