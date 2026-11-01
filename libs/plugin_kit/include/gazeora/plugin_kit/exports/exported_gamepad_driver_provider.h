// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_switch.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: one of Gazeora's own IGamepadDriverFactory, as a
// GazeoraGamepadDriverProviderV3 for a bundled plugin to register, with the plugin's switch
// when it has one. The struct points back at it, so it can't be copied or moved.
class ExportedGamepadDriverProvider final {
public:
    // `enabled` may be null: controllers are then always read.
    ExportedGamepadDriverProvider(std::string name,
                                  std::shared_ptr<input::IGamepadDriverFactory> drivers,
                                  std::shared_ptr<const input::IGamepadSwitch> enabled = nullptr);
    ExportedGamepadDriverProvider(const ExportedGamepadDriverProvider&) = delete;
    ExportedGamepadDriverProvider& operator=(const ExportedGamepadDriverProvider&) = delete;

    const GazeoraGamepadDriverProviderV3& Provider() const { return m_provider; }

private:
    static int32_t GAZEORA_CALL Create(void* self, GazeoraGamepadDriverV3* driver, char* error,
                                       size_t capacity);
    static int32_t GAZEORA_CALL Enabled(void* self);

    const std::string m_name;
    const std::shared_ptr<input::IGamepadDriverFactory> m_drivers;
    const std::shared_ptr<const input::IGamepadSwitch> m_enabled;
    GazeoraGamepadDriverProviderV3 m_provider = {};
};

}  // namespace gazeora
