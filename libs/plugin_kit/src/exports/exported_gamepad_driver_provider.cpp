// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_gamepad_driver_provider.h"

#include <exception>

#include "gazeora/input/exceptions/input_error.h"
#include "gazeora/plugin_kit/exports/exported_gamepad_driver.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

constexpr char kUnexpected[] = "Game controllers stopped working unexpectedly.";

}  // namespace

ExportedGamepadDriverProvider::ExportedGamepadDriverProvider(
    std::string name, std::shared_ptr<input::IGamepadDriverFactory> drivers,
    std::shared_ptr<const input::IGamepadSwitch> enabled)
    : m_name(std::move(name)), m_drivers(std::move(drivers)), m_enabled(std::move(enabled)) {
    m_provider.struct_size = sizeof(m_provider);
    m_provider.self = this;
    m_provider.name = m_name.c_str();
    m_provider.create = Create;
    m_provider.enabled = m_enabled ? Enabled : nullptr;
}

int32_t GAZEORA_CALL ExportedGamepadDriverProvider::Enabled(void* self) {
    const auto& provider = *static_cast<const ExportedGamepadDriverProvider*>(self);
    return provider.m_enabled->Enabled() ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL ExportedGamepadDriverProvider::Create(void* self,
                                                           GazeoraGamepadDriverV3* driver,
                                                           char* error, size_t capacity) {
    if (!driver) {
        return GAZEORA_FALSE;
    }
    const auto& provider = *static_cast<const ExportedGamepadDriverProvider*>(self);
    // Nothing may throw out of a plugin function.
    try {
        ExportedGamepadDriver::Fill(provider.m_drivers->Create(), *driver);
        return GAZEORA_TRUE;
    } catch (const input::InputError& failure) {
        PluginText::Write(failure.what(), error, capacity);
    } catch (const std::exception&) {
        PluginText::Write(kUnexpected, error, capacity);
    }
    return GAZEORA_FALSE;
}

}  // namespace gazeora
