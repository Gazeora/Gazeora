// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "sample_parts.h"

namespace sample {

namespace {

constexpr char kControllerName[] = "Sample controller";

// One controller, holding the bottom face button with its left stick pushed right.
struct Driver {
    int updates = 0;
};

void GAZEORA_CALL Update(void* self) { ++static_cast<Driver*>(self)->updates; }

void GAZEORA_CALL Connected(void*, GazeoraGamepadNameSinkV3 sink, void* sinkContext) {
    sink(sinkContext, kControllerName);
}

void GAZEORA_CALL State(void*, GazeoraGamepadStateV3* state) {
    state->buttons = 1u << GAZEORA_GAMEPAD_BUTTON_SOUTH;
    state->axes[GAZEORA_GAMEPAD_AXIS_LEFT_X] = 1.0f;
}

void GAZEORA_CALL Destroy(void* self) { delete static_cast<Driver*>(self); }

int32_t GAZEORA_CALL Create(void*, GazeoraGamepadDriverV3* driver, char*, size_t) {
    *driver = {sizeof(GazeoraGamepadDriverV3), new Driver, Update, Connected, State, Destroy};
    return GAZEORA_TRUE;
}

const GazeoraGamepadDriverProviderV3 kGamepads = {sizeof(GazeoraGamepadDriverProviderV3), nullptr,
                                                  "Sample controllers", Create};

}  // namespace

const GazeoraGamepadDriverProviderV3& Gamepads() { return kGamepads; }

}  // namespace sample
