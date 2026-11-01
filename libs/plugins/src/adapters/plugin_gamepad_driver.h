// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "gazeora/input/interfaces/gamepad_driver.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraGamepadDriverV3 behind IGamepadDriver. Keeps the DLL
// loaded and destroys the plugin's driver with it.
class PluginGamepadDriver final : public input::IGamepadDriver {
public:
    // `driver` must have passed Valid().
    PluginGamepadDriver(std::shared_ptr<const IPluginModule> module,
                        const GazeoraGamepadDriverV3& driver);
    PluginGamepadDriver(const PluginGamepadDriver&) = delete;
    PluginGamepadDriver& operator=(const PluginGamepadDriver&) = delete;
    ~PluginGamepadDriver() override;

    // Whether the plugin filled in every function a driver must have.
    static bool Valid(const GazeoraGamepadDriverV3& driver);

    void Update() override;
    std::vector<input::GamepadInfo> Connected() const override;
    input::GamepadState State() const override;

private:
    static void GAZEORA_CALL CollectName(void* context, const char* name);

    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraGamepadDriverV3 m_driver;
};

}  // namespace gazeora
