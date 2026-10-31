// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "gazeora/input/exceptions/input_error.h"
#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_listener.h"

namespace gazeora::input::testing {

// Controllers a test plugs in and moves, read by a session's thread.
class FakeGamepads {
public:
    void Connect(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected.push_back({name});
    }
    void Set(const GamepadState& state) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = state;
    }
    std::vector<GamepadInfo> Connected() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connected;
    }
    GamepadState State() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<GamepadInfo> m_connected;
    GamepadState m_state;
};

class FakeGamepadDriver final : public IGamepadDriver {
public:
    explicit FakeGamepadDriver(const FakeGamepads& gamepads) : m_gamepads(gamepads) {}

    void Update() override {}
    std::vector<GamepadInfo> Connected() const override { return m_gamepads.Connected(); }
    GamepadState State() const override { return m_gamepads.State(); }

private:
    const FakeGamepads& m_gamepads;
};

// Makes drivers onto `gamepads`, or fails with `failure` when it's set.
class FakeGamepadDriverFactory final : public IGamepadDriverFactory {
public:
    explicit FakeGamepadDriverFactory(const FakeGamepads& gamepads) : m_gamepads(gamepads) {}

    std::unique_ptr<IGamepadDriver> Create() override {
        if (!failure.empty()) {
            throw InputError(failure);
        }
        return std::make_unique<FakeGamepadDriver>(m_gamepads);
    }

    std::wstring failure;

private:
    const FakeGamepads& m_gamepads;
};

// Remembers every state it was told, in order.
class RecordingGamepadListener final : public IGamepadListener {
public:
    void OnGamepadState(const GamepadState& state) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_states.push_back(state);
    }
    std::vector<GamepadState> States() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_states;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<GamepadState> m_states;
};

}  // namespace gazeora::input::testing
