// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/foreground_game_focus.h"

namespace gazeora::app {

bool ForegroundGameFocus::GameInFront() const {
    const DWORD game = m_pid;
    HWND front = GetForegroundWindow();
    if (game == 0 || !front) {
        return false;
    }
    DWORD owner = 0;
    GetWindowThreadProcessId(front, &owner);
    return owner == game;
}

}  // namespace gazeora::app
