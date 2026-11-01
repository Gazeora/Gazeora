// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"

namespace gazeora::ui {

// <toggle>: an on/off switch for the value under `bind` ("1" or "0"), its label beside it.
struct UiToggle : UiVisitable<UiToggle> {
    std::string bind;
    std::string label;
    std::string help;  // behind a "(?)" after the label; none if empty
};

}  // namespace gazeora::ui
