// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gamepad_ui_values.h"

#include <memory>

#include "plugin_ui/gamepad_status_cache.h"

namespace gazeora::app {

namespace {

constexpr char kStatus[] = "gazeora.gamepad.status";
constexpr char kTone[] = "gazeora.gamepad.tone";
constexpr char kFailed[] = "gazeora.gamepad.failed";
constexpr char kRetry[] = "gazeora.gamepad.retry";

}  // namespace

void GamepadUiValues::AddTo(GazeoraUiValues& values, GamepadControl& control) {
    // Shared by the getters, which keep it alive.
    auto status = std::make_shared<GamepadStatusCache>(control);
    values.AddValue(kStatus, [status] { return status->Line().Text(); });
    values.AddValue(kTone, [status] { return ui::UiTones::IdOf(status->Line().Tone()); });
    values.AddValue(kFailed, [status] { return GazeoraUiValues::Flag(status->Failed()); });
    values.AddAction(kRetry, [&control] { control.Restart(); });
}

}  // namespace gazeora::app
