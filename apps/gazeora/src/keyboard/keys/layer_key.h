// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// Switches between the letters and the symbols; `label` names the other one.
class LayerKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 2.0f;

    explicit LayerKey(const char* label) : m_label(label) {}

    std::string Label(bool) const override { return m_label; }
    void Press(IKeyboardActions& actions) const override { actions.ToggleSymbols(); }
    float Span() const override { return kSpan; }

private:
    const char* m_label;
};

}  // namespace gazeora::app
