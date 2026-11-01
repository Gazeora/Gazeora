// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_switch.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraGamepadDriverProviderV3 behind IGamepadDriverFactory, and
// its switch behind IGamepadSwitch. Keeps the DLL loaded.
class PluginGamepadDriverFactory final : public input::IGamepadDriverFactory,
                                         public input::IGamepadSwitch {
public:
    // `provider` must have passed PluginRegistrar's checks.
    PluginGamepadDriverFactory(std::shared_ptr<const IPluginModule> module,
                               const GazeoraGamepadDriverProviderV3& provider);

    // Throws InputError with the plugin's reason if it couldn't make a driver.
    std::unique_ptr<input::IGamepadDriver> Create() override;
    // Always, for a provider without a switch.
    bool Enabled() const override;

private:
    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraGamepadDriverProviderV3 m_provider;
    const std::wstring m_name;
};

}  // namespace gazeora
