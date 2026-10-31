// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include "gazeora/hook/input/stick_turn.h"

namespace gazeora::hook::input {

// The game read a connected controller, through `path` (ControllerPath), `active` if the player
// is using it: tells the host, and gives the push for the head's turn since the last read (none
// while the host sends head turns to the mouse). From any of the game's threads.
StickPush HeadTurnForRead(bool active, LONG path);

}  // namespace gazeora::hook::input
