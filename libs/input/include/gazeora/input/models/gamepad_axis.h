#pragma once

namespace gazeora::input {

// A controller's sticks, each from -1 to 1: X positive to the right, Y positive down.
enum class GamepadAxis { LeftX, LeftY, RightX, RightY };

constexpr int kGamepadAxisCount = int(GamepadAxis::RightY) + 1;

}  // namespace gazeora::input
