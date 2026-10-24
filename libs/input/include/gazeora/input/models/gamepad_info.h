#pragma once

#include <string>

namespace gazeora::input {

// A game controller connected to this PC.
struct GamepadInfo {
    std::wstring name;  // for people, e.g. "DualSense Wireless Controller"

    bool operator==(const GamepadInfo& other) const { return name == other.name; }
    bool operator!=(const GamepadInfo& other) const { return !(*this == other); }
};

}  // namespace gazeora::input
