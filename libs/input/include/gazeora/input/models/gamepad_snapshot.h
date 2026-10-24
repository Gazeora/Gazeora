#pragma once

#include <string>
#include <vector>

#include "gazeora/input/models/gamepad_info.h"

namespace gazeora::input {

enum class GamepadSessionState { Off, Running, Error };

// What reading game controllers is doing right now, for the UI.
struct GamepadSnapshot {
    GamepadSessionState state = GamepadSessionState::Off;
    std::vector<GamepadInfo> connected;
    std::wstring message;  // why, in the Error state
};

}  // namespace gazeora::input
