// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <atomic>

#include "head/game_focus.h"

namespace gazeora::app {

// The running game's process, against the process of Windows' foreground window, checked
// each time it's asked, so switching away from the game stops the head at once.
class ForegroundGameFocus final : public GameFocus {
public:
    // The running game's process id; 0 while none runs. From any thread.
    void SetGame(DWORD pid) { m_pid = pid; }

    bool GameInFront() const override;

private:
    std::atomic<DWORD> m_pid{0};
};

}  // namespace gazeora::app
