// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "keyboard/keyboard_actions.h"

namespace gazeora::app {

// Command pattern: a key of the on-screen keyboard, and what pressing it does.
class IKeyboardKey {
public:
    virtual ~IKeyboardKey() = default;

    // What it shows, in UTF-8.
    virtual std::string Label(bool shifted) const = 0;
    virtual void Press(IKeyboardActions& actions) const = 0;
    // How many ordinary keys wide it is.
    virtual float Span() const { return 1.0f; }
};

}  // namespace gazeora::app
