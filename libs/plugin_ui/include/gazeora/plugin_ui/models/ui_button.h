// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"
#include "gazeora/plugin_ui/models/ui_icon.h"

namespace gazeora::ui {

// <button>: does `action` when pressed.
struct UiButton : UiVisitable<UiButton> {
    std::string action;
    std::string label;
    UiIcon icon = UiIcon::None;
};

}  // namespace gazeora::ui
