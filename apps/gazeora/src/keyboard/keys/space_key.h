// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

class SpaceKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 4.0f;

    std::string Label(bool) const override { return "Space"; }
    void Press(IKeyboardActions& actions) const override { actions.Type(U' '); }
    float Span() const override { return kSpan; }
};

}  // namespace gazeora::app
