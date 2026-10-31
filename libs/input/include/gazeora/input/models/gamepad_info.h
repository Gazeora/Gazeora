// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::input {

// A game controller connected to this PC.
struct GamepadInfo {
    std::wstring name;  // for people, e.g. "DualSense Wireless Controller"

    bool operator==(const GamepadInfo& other) const { return name == other.name; }
    bool operator!=(const GamepadInfo& other) const { return !(*this == other); }
};

}  // namespace gazeora::input
