// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gamepad_ui_values.h"

#include "ui/gamepad_status_line.h"

namespace gazeora::app {

namespace {

constexpr char kStatus[] = "gazeora.gamepad.status";
constexpr char kTone[] = "gazeora.gamepad.tone";
constexpr char kFailed[] = "gazeora.gamepad.failed";
constexpr char kRetry[] = "gazeora.gamepad.retry";

GamepadStatusLine StatusOf(const GamepadControl& control) {
    return GamepadStatusLine(control.Snapshot(), control.Paused());
}

}  // namespace

void GamepadUiValues::AddTo(GazeoraUiValues& values, GamepadControl& control) {
    values.AddValue(kStatus, [&control] { return StatusOf(control).Text(); });
    values.AddValue(kTone, [&control] { return ui::UiTones::IdOf(StatusOf(control).Tone()); });
    values.AddValue(kFailed, [&control] {
        return GazeoraUiValues::Flag(control.Snapshot().state == input::GamepadSessionState::Error);
    });
    values.AddAction(kRetry, [&control] { control.Restart(); });
}

}  // namespace gazeora::app
