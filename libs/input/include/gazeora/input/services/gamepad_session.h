// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "gazeora/input/gamepad/gamepad_state_filter.h"
#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_listener.h"
#include "gazeora/input/models/gamepad_snapshot.h"

namespace gazeora::input {

// Game controllers read in the background: a driver made on a thread of its own, polled
// until stopped. The listener hears each change, and a neutral state when it stops, so
// nothing stays held.
class GamepadSession {
public:
    GamepadSession(IGamepadDriverFactory& drivers, IGamepadListener& listener);
    ~GamepadSession();

    GamepadSession(const GamepadSession&) = delete;
    GamepadSession& operator=(const GamepadSession&) = delete;

    // Starts over, stopping a session already running.
    void Start();
    void Stop();
    bool Running() const { return m_thread.joinable(); }
    GamepadSnapshot Snapshot() const;

private:
    void Run();
    void Loop(IGamepadDriver& driver);
    void Poll(IGamepadDriver& driver);
    // Tells the listener, if the state changed.
    void Report(const GamepadState& state);
    void PublishConnected(std::vector<GamepadInfo> connected);
    void Publish(GamepadSnapshot snapshot);
    void Fail(std::wstring message);

    IGamepadDriverFactory& m_drivers;
    IGamepadListener& m_listener;
    const GamepadStateFilter m_filter;

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    GamepadState m_reported;  // the session's thread only
    mutable std::mutex m_mutex;
    GamepadSnapshot m_snapshot;  // guarded by m_mutex
};

}  // namespace gazeora::input
