#pragma once

#include "gazeora/input/models/gamepad_state.h"

namespace gazeora::input {

// Observer pattern: told each time what the controllers do changes, on the session's thread.
class IGamepadListener {
public:
    virtual ~IGamepadListener() = default;

    virtual void OnGamepadState(const GamepadState& state) = 0;
};

}  // namespace gazeora::input
