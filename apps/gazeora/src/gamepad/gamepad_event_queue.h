#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <vector>

#include "gazeora/input/interfaces/gamepad_listener.h"

namespace gazeora::app {

// Observer pattern: hands what the controllers do from their session's thread to the UI
// thread, which alone may feed it to the UI, and wakes it so it shows at once.
class GamepadEventQueue final : public input::IGamepadListener {
public:
    // `wake` is called from the session's thread after each change.
    explicit GamepadEventQueue(std::function<void()> wake) : m_wake(std::move(wake)) {}

    void OnGamepadState(const input::GamepadState& state) override;
    // The states since the last call, oldest first.
    std::vector<input::GamepadState> Take();

private:
    std::function<void()> m_wake;
    std::mutex m_mutex;
    std::deque<input::GamepadState> m_states;
};

}  // namespace gazeora::app
