// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
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
// nothing stays held. Without drivers (no plugin offers them) it never starts and stays
// Unavailable.
class GamepadSession {
public:
    // `drivers` may be null; `listener` must outlive this.
    GamepadSession(std::shared_ptr<IGamepadDriverFactory> drivers, IGamepadListener& listener);
    ~GamepadSession();

    GamepadSession(const GamepadSession&) = delete;
    GamepadSession& operator=(const GamepadSession&) = delete;

    // Starts over, stopping a session already running. Does nothing without drivers.
    void Start();
    void Stop();
    bool Running() const { return m_thread.joinable(); }
    bool Available() const { return m_drivers != nullptr; }
    GamepadSnapshot Snapshot() const;
    // Goes up each time Snapshot() would say something new, so a caller can keep what it made
    // of the last one instead of copying it again.
    uint64_t Version() const { return m_version.load(std::memory_order_acquire); }

private:
    void Run();
    void Loop(IGamepadDriver& driver);
    void Poll(IGamepadDriver& driver);
    // Tells the listener, if the state changed.
    void Report(const GamepadState& state);
    void PublishConnected(std::vector<GamepadInfo> connected);
    void Publish(GamepadSnapshot snapshot);
    void Fail(std::wstring message);
    // What it is while not running: Off, or Unavailable without drivers.
    GamepadSnapshot Idle() const;

    const std::shared_ptr<IGamepadDriverFactory> m_drivers;
    IGamepadListener& m_listener;
    const GamepadStateFilter m_filter;

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    GamepadState m_reported;  // the session's thread only
    mutable std::mutex m_mutex;
    GamepadSnapshot m_snapshot;  // guarded by m_mutex
    std::atomic<uint64_t> m_version{0};
};

}  // namespace gazeora::input
