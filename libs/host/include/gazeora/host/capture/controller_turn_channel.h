// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <cstdint>
#include <mutex>

#include "gazeora/common/shared.h"

namespace gazeora {

// Head turns for a game's controller (common/shared.h): the running game's shared block, opened
// by its process id beside the capture client that made it, so the hook in the game can turn
// them into right-stick movement. Without the hook in the game (capture off), there's no block
// and nothing happens. From any thread.
class ControllerTurnChannel {
public:
    // What the hook has seen the game do with its controllers.
    struct Reads {
        bool recent = false;     // the game read a connected controller in the last second
        LONGLONG activeQpc = 0;  // when the player last used it (QueryPerformanceCounter)
        LONG paths = 0;          // ControllerPath bits
    };

    ControllerTurnChannel() = default;
    ~ControllerTurnChannel() { Attach(0); }
    ControllerTurnChannel(const ControllerTurnChannel&) = delete;
    ControllerTurnChannel& operator=(const ControllerTurnChannel&) = delete;

    // The game with process `pid`; 0 for none. Nothing happens if it's the one attached.
    void Attach(DWORD pid);
    bool Attached() const;

    // Adds a turn of the head: to the left and up, in degrees.
    void AddTurn(double yawDegrees, double pitchDegrees);
    // How the game turns with the stick: degrees a second at full tilt, and its dead zone 0..1.
    // Kept, and written to every game attached.
    void SetStick(float degreesPerSecond, float deadZone);
    Reads Read() const;

private:
    void Detach();
    void WriteStick();

    mutable std::mutex m_mutex;
    DWORD m_pid = 0;
    HANDLE m_mapping = nullptr;
    SharedHeader* m_shared = nullptr;
    float m_degreesPerSecond = 0.0f;
    float m_deadZone = 0.0f;
};

}  // namespace gazeora
