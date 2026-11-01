// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_gamepad_driver.h"

#include "gazeora/plugin_kit/adapters/plugin_gamepad_state.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

constexpr wchar_t kUnnamed[] = L"Game controller";

}  // namespace

PluginGamepadDriver::PluginGamepadDriver(std::shared_ptr<const IPluginModule> module,
                                         const GazeoraGamepadDriverV3& driver)
    : m_module(std::move(module)), m_driver(driver) {}

PluginGamepadDriver::~PluginGamepadDriver() { m_driver.destroy(m_driver.self); }

bool PluginGamepadDriver::Valid(const GazeoraGamepadDriverV3& driver) {
    return driver.struct_size >= sizeof(GazeoraGamepadDriverV3) && driver.update &&
           driver.connected && driver.state && driver.destroy;
}

void PluginGamepadDriver::Update() { m_driver.update(m_driver.self); }

std::vector<input::GamepadInfo> PluginGamepadDriver::Connected() const {
    std::vector<input::GamepadInfo> connected;
    m_driver.connected(m_driver.self, CollectName, &connected);
    return connected;
}

input::GamepadState PluginGamepadDriver::State() const {
    GazeoraGamepadStateV3 state = {};
    state.struct_size = sizeof(state);
    m_driver.state(m_driver.self, &state);
    return PluginGamepadState::FromPlugin(state);
}

void GAZEORA_CALL PluginGamepadDriver::CollectName(void* context, const char* name) {
    auto& connected = *static_cast<std::vector<input::GamepadInfo>*>(context);
    std::wstring wide = PluginText::Wide(name);
    connected.push_back({wide.empty() ? kUnnamed : std::move(wide)});
}

}  // namespace gazeora
