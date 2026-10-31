// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// Types a character: `shifted` while Shift is on.
class CharacterKey final : public IKeyboardKey {
public:
    CharacterKey(char plain, char shifted) : m_plain(plain), m_shifted(shifted) {}

    std::string Label(bool shifted) const override {
        return std::string(1, shifted ? m_shifted : m_plain);
    }
    void Press(IKeyboardActions& actions) const override {
        actions.Type(char32_t(actions.Shifted() ? m_shifted : m_plain));
    }

private:
    char m_plain;
    char m_shifted;
};

}  // namespace gazeora::app
