// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <atomic>

namespace gazeora::app {

// When the player last used the PC's own keyboard or mouse, whatever window is in front: raw
// input, which marks what programs inject (head turns sent as mouse moves among them) as from
// no device, so only the player's own count.
class KeyboardMouseWatch {
public:
    // Asks for `hwnd`'s raw keyboard and mouse input, in the background too.
    void Register(HWND hwnd);
    // A WM_INPUT message's.
    void OnRawInput(HRAWINPUT input);
    // QueryPerformanceCounter's time of the last use; 0 if none yet. From any thread.
    LONGLONG LastUsedQpc() const { return m_lastUsedQpc; }

private:
    std::atomic<LONGLONG> m_lastUsedQpc{0};
};

}  // namespace gazeora::app
