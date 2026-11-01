// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_ui/interfaces/ui_values.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraUiPanelV3 functions behind IUiValues. Keeps the DLL
// loaded.
class PluginUiValues final : public ui::IUiValues {
public:
    // `panel` must have passed PluginRegistrar's checks.
    PluginUiValues(std::shared_ptr<const IPluginModule> module, const GazeoraUiPanelV3& panel)
        : m_module(std::move(module)), m_panel(panel) {}

    std::string Get(const std::string& key) const override;
    void Set(const std::string& key, const std::string& value) override;
    void Act(const std::string& action) override;

private:
    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraUiPanelV3 m_panel;
};

}  // namespace gazeora
