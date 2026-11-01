// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "controller_settings.h"
#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora {

// What the plugin's card (controller_panel.xml) binds to besides Gazeora's own values: its
// switch, as "enabled".
class ControllerPanelValues final : public ui::IUiValues {
public:
    explicit ControllerPanelValues(std::shared_ptr<ControllerSettings> settings)
        : m_settings(std::move(settings)) {}

    std::string Get(const std::string& key) const override;
    void Set(const std::string& key, const std::string& value) override;
    // The card has no actions of its own.
    void Act(const std::string&) override {}

private:
    const std::shared_ptr<ControllerSettings> m_settings;
};

}  // namespace gazeora
