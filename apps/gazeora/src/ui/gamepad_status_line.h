#pragma once

#include <imgui.h>

#include <string>

#include "gazeora/input/models/gamepad_snapshot.h"

namespace gazeora::app {

// The one line under the game controller switch: which controllers are connected, or why
// none is read.
class GamepadStatusLine {
public:
    GamepadStatusLine(const input::GamepadSnapshot& snapshot, bool paused);

    const std::string& Text() const { return m_text; }
    ImU32 Color() const { return m_color; }

private:
    void Describe(const input::GamepadSnapshot& snapshot);
    void DescribeConnected(const input::GamepadSnapshot& snapshot);

    std::string m_text;
    ImU32 m_color;
};

}  // namespace gazeora::app
