// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora::ui {

// Proxy pattern: what one panel binds to. Keys and actions starting with "gazeora." go to
// Gazeora's own values, the rest to the plugin's. Both must outlive this.
class UiBindings final : public IUiValues {
public:
    static constexpr char kGazeoraPrefix[] = "gazeora.";

    UiBindings(IUiValues& gazeora, IUiValues& plugin) : m_gazeora(gazeora), m_plugin(plugin) {}

    std::string Get(const std::string& key) const override;
    void Set(const std::string& key, const std::string& value) override;
    void Act(const std::string& action) override;

    static bool IsGazeoras(const std::string& key);

private:
    IUiValues& For(const std::string& key) const;

    IUiValues& m_gazeora;
    IUiValues& m_plugin;
};

}  // namespace gazeora::ui
