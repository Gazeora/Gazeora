// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_activation.h"

#include <optional>
#include <utility>

namespace gazeora::app {

PluginActivation::PluginActivation(PluginServices& plugins, Changed changed)
    : m_plugins(plugins), m_changed(std::move(changed)) {}

bool PluginActivation::Activate(const std::wstring& fileName) {
    const PluginStatus* added = m_plugins.Add(fileName);
    if (!added) {
        return false;
    }
    m_changed(*added, true);
    return true;
}

bool PluginActivation::Deactivate(const std::wstring& fileName) {
    const std::optional<PluginStatus> removed = m_plugins.Remove(fileName);
    if (!removed) {
        return false;
    }
    m_changed(*removed, false);
    return true;
}

}  // namespace gazeora::app
