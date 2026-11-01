// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/input/interfaces/gamepad_driver.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: one of Gazeora's own IGamepadDriver, as the GazeoraGamepadDriverV3 a
// bundled plugin hands Gazeora. The struct's destroy frees it.
class ExportedGamepadDriver final {
public:
    ExportedGamepadDriver() = delete;

    // Fills `driver` with `exported`'s functions.
    static void Fill(std::unique_ptr<input::IGamepadDriver> exported,
                     GazeoraGamepadDriverV3& driver);

private:
    static input::IGamepadDriver& Of(void* self) {
        return *static_cast<input::IGamepadDriver*>(self);
    }

    static void GAZEORA_CALL Update(void* self);
    static void GAZEORA_CALL Connected(void* self, GazeoraGamepadNameSinkV3 sink,
                                       void* sinkContext);
    static void GAZEORA_CALL State(void* self, GazeoraGamepadStateV3* state);
    static void GAZEORA_CALL Destroy(void* self);
};

}  // namespace gazeora
