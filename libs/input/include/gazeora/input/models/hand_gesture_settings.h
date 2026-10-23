#pragma once

#include <string>

namespace gazeora::input {

// What the user chose for hand gesture control.
struct HandGestureSettings {
    static constexpr float kMinSpeed = 1.0f;
    static constexpr float kMaxSpeed = 10.0f;
    static constexpr float kDefaultSpeed = 5.0f;

    bool enabled = false;
    std::wstring cameraId;  // empty: the first camera
    // How far the pointer goes for a fingertip move, like a mouse's speed: at 5, moving the
    // fingertip by a palm's length sweeps the screen and a quarter.
    float speed = kDefaultSpeed;
};

}  // namespace gazeora::input
