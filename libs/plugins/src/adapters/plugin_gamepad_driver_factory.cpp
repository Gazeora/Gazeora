// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_gamepad_driver_factory.h"

#include "adapters/plugin_gamepad_driver.h"
#include "adapters/plugin_object_guard.h"
#include "gazeora/input/exceptions/input_error.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginGamepadDriverFactory::PluginGamepadDriverFactory(
    std::shared_ptr<const IPluginModule> module, const GazeoraGamepadDriverProviderV3& provider)
    : m_module(std::move(module)), m_provider(provider), m_name(PluginText::Wide(provider.name)) {}

std::unique_ptr<input::IGamepadDriver> PluginGamepadDriverFactory::Create() {
    GazeoraGamepadDriverV3 driver = {};
    driver.struct_size = sizeof(driver);
    PluginText error;
    if (m_provider.create(m_provider.self, &driver, error.Data(), error.Capacity()) !=
        GAZEORA_TRUE) {
        const std::wstring why = error.Wide();
        throw input::InputError(why.empty() ? L"Game controllers can't be read through " + m_name
                                            : why);
    }
    PluginObjectGuard<GazeoraGamepadDriverV3> guard(driver);
    if (!PluginGamepadDriver::Valid(driver)) {
        throw input::InputError(m_name + L" made an incomplete game controller driver");
    }
    auto adapted = std::make_unique<PluginGamepadDriver>(m_module, driver);
    guard.Release();
    return adapted;
}

bool PluginGamepadDriverFactory::Enabled() const {
    return !m_provider.enabled || m_provider.enabled(m_provider.self) == GAZEORA_TRUE;
}

}  // namespace gazeora
