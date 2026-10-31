// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::hook::input {

// Hooks the ways games read controllers, so head tracking can turn the camera with the right
// stick (head_turn_feed.h) rather than the mouse, which would switch the game's prompts and
// controls to keyboard and mouse: Sony controllers on USB through their HID reports (ReadFile,
// GetOverlappedResult, HidD_GetInputReport), and XInput controllers through XInputGetState.
// The hooks are created, not enabled.
bool Install();

}  // namespace gazeora::hook::input
