// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// Deletes the character before the caret.
class EraseKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 2.0f;

    std::string Label(bool) const override { return "Delete"; }
    void Press(IKeyboardActions& actions) const override { actions.Erase(); }
    float Span() const override { return kSpan; }
};

}  // namespace gazeora::app
