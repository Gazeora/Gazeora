// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/plugin_ui/models/ui_element.h"

namespace gazeora::ui {

// <group>: elements shown or enabled together; one under another, or side by side as a row.
struct UiGroup : UiVisitable<UiGroup> {
    UiElements children;
    bool row = false;
};

}  // namespace gazeora::ui
