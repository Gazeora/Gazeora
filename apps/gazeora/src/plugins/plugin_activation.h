// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <string>

#include "gazeora/plugins/plugin_services.h"
#include "plugins/plugin_activator.h"

namespace gazeora::app {

// Observer pattern: puts installs and uninstalls into effect without a restart. Adds the plugin
// to PluginServices or takes it out, then tells `changed`, which hands the new registries to
// what uses them. A plugin whose file was loaded before since start can't be loaded again, so
// an update or reinstall of one still waits for the next start.
class PluginActivation final : public IPluginActivator {
public:
    // `plugin` is how the plugin loaded (Activate), or had loaded (Deactivate); `added` tells
    // which.
    using Changed = std::function<void(const PluginStatus& plugin, bool added)>;

    // `plugins` must outlive this.
    PluginActivation(PluginServices& plugins, Changed changed);

    bool Activate(const std::wstring& fileName) override;
    bool Deactivate(const std::wstring& fileName) override;

private:
    PluginServices& m_plugins;
    const Changed m_changed;
};

}  // namespace gazeora::app
