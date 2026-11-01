// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_layout.h"

namespace gazeora::ui {

// Reads a plugin's panel layout from its XML (docs/plugin-ui.md). Strict, so a mistake shows
// up when the plugin loads rather than as a card that looks wrong: an unknown element or
// attribute, or a missing one, is refused.
class UiXmlParser final {
public:
    UiXmlParser() = delete;

    // Throws UiLayoutError, saying what's wrong and on which line.
    static UiLayout Parse(const std::string& xml);
};

}  // namespace gazeora::ui
