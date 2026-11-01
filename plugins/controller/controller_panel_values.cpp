// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "controller_panel_values.h"

#include "gazeora/plugin_ui/models/ui_condition.h"

namespace gazeora {

namespace {

constexpr char kEnabled[] = "enabled";
constexpr char kOn[] = "1";
constexpr char kOff[] = "0";

}  // namespace

std::string ControllerPanelValues::Get(const std::string& key) const {
    if (key != kEnabled) {
        return std::string();
    }
    return m_settings->Enabled() ? kOn : kOff;
}

void ControllerPanelValues::Set(const std::string& key, const std::string& value) {
    if (key == kEnabled) {
        m_settings->SetEnabled(ui::UiCondition::IsTrue(value));
    }
}

}  // namespace gazeora
