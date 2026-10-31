// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// The next character in capitals, or the digits' symbols.
class ShiftKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 2.0f;

    std::string Label(bool shifted) const override { return shifted ? "SHIFT" : "Shift"; }
    void Press(IKeyboardActions& actions) const override { actions.ToggleShift(); }
    float Span() const override { return kSpan; }
};

}  // namespace gazeora::app
