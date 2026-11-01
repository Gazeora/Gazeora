// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugins/interfaces/plugin_module_loader.h"
#include "gazeora/plugins/models/plugin_catalog.h"

namespace gazeora {

// Loads plugin DLLs, checks they're for this plugin API version, and collects what they
// register. A plugin that fails to load is reported in the catalog and skipped.
class PluginLoader final {
public:
    // `settingsFolder` is where plugins may keep their settings; empty for none.
    explicit PluginLoader(const IPluginModuleLoader& modules, std::wstring settingsFolder = L"")
        : m_modules(modules), m_settingsFolder(std::move(settingsFolder)) {}

    PluginCatalog Load(const std::vector<std::wstring>& paths) const;
    // One plugin: what it offered and its status, also when it didn't load.
    PluginCatalog LoadOne(const std::wstring& path) const;

private:
    // Reads the plugin's info into `status`. False with status.error set if it's no plugin
    // of this API version.
    static bool ReadInfo(const IPluginModule& module, PluginStatus& status);
    // Counts in `status` what one plugin registered.
    static void Count(const PluginCatalog& registered, PluginStatus& status);

    const IPluginModuleLoader& m_modules;
    const std::wstring m_settingsFolder;
};

}  // namespace gazeora
