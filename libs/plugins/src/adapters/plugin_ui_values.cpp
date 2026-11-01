// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_ui_values.h"

#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

std::string PluginUiValues::Get(const std::string& key) const {
    PluginText value;
    m_panel.get(m_panel.self, key.c_str(), value.Data(), value.Capacity());
    return Utf8::FromWide(value.Wide());
}

void PluginUiValues::Set(const std::string& key, const std::string& value) {
    m_panel.set(m_panel.self, key.c_str(), value.c_str());
}

void PluginUiValues::Act(const std::string& action) {
    m_panel.action(m_panel.self, action.c_str());
}

}  // namespace gazeora
