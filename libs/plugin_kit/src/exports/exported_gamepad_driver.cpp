// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_gamepad_driver.h"

#include <exception>
#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_kit/adapters/plugin_gamepad_state.h"
#include "gazeora/plugin_kit/utils/plugin_struct.h"

namespace gazeora {

namespace {

// Nothing may throw out of a plugin function: a failed call reports nothing new.
template <typename Call>
void Guarded(Call call) {
    try {
        call();
    } catch (const std::exception&) {
    }
}

}  // namespace

void ExportedGamepadDriver::Fill(std::unique_ptr<input::IGamepadDriver> exported,
                                 GazeoraGamepadDriverV3& driver) {
    driver = {
        sizeof(GazeoraGamepadDriverV3), exported.release(), Update, Connected, State, Destroy};
}

void GAZEORA_CALL ExportedGamepadDriver::Update(void* self) {
    Guarded([&] { Of(self).Update(); });
}

void GAZEORA_CALL ExportedGamepadDriver::Connected(void* self, GazeoraGamepadNameSinkV3 sink,
                                                   void* sinkContext) {
    if (!sink) {
        return;
    }
    Guarded([&] {
        for (const input::GamepadInfo& gamepad : Of(self).Connected()) {
            sink(sinkContext, Utf8::FromWide(gamepad.name).c_str());
        }
    });
}

void GAZEORA_CALL ExportedGamepadDriver::State(void* self, GazeoraGamepadStateV3* state) {
    if (!state || !PluginStruct::Covers<GazeoraGamepadStateV3>(state->struct_size)) {
        return;
    }
    Guarded([&] { *state = PluginGamepadState::ToPlugin(Of(self).State()); });
}

void GAZEORA_CALL ExportedGamepadDriver::Destroy(void* self) {
    delete static_cast<input::IGamepadDriver*>(self);
}

}  // namespace gazeora
