// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora::ui {

// When an element is shown or can be used: while the value under a key is true, or with '!'
// in front, false. A value is false if it's "", "0" or "false". Always holds without a key.
class UiCondition {
public:
    UiCondition() = default;
    // From a layout's attribute, e.g. "enabled" or "!gazeora.game-running".
    explicit UiCondition(const std::string& text);

    bool Holds(const IUiValues& values) const;
    static bool IsTrue(const std::string& value);

private:
    std::string m_key;
    bool m_negated = false;
};

}  // namespace gazeora::ui
