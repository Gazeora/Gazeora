// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"
#include "gazeora/plugin_ui/models/ui_icon.h"
#include "gazeora/plugin_ui/models/ui_page.h"
#include "gazeora/plugin_ui/models/ui_position.h"

namespace gazeora::ui {

// A <panel>: a card on a page, with a title and its elements one under another.
struct UiLayout {
    UiPage page = UiPage::Settings;
    UiPosition position = UiPosition::End;
    std::string title;
    UiIcon icon = UiIcon::None;
    UiElements children;
};

}  // namespace gazeora::ui
