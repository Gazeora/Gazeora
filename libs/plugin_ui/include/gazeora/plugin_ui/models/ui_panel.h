// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugin_ui/interfaces/ui_values.h"
#include "gazeora/plugin_ui/models/ui_layout.h"

namespace gazeora::ui {

// A plugin's card: its layout, and the values behind it, which keep the plugin loaded.
struct UiPanel {
    std::string id;  // unique among the panels, for the UI's widget ids
    UiLayout layout;
    std::shared_ptr<IUiValues> values;
};

}  // namespace gazeora::ui
