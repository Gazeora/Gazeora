#pragma once

#include <imgui.h>

#include <array>
#include <vector>

#include "gazeora/input/models/gamepad_state.h"

namespace gazeora::app {

// Adapter pattern: game controllers as ImGui's gamepad navigation, onto this window's UI only.
// The d-pad or the left stick moves between controls, the bottom button presses, the right
// one backs out; the right stick goes through as ImGui keys, for the pages to scroll with.
class ImGuiGamepadInput {
public:
    // How many ImGui keys the controllers drive.
    static constexpr size_t kKeyCount = 10;

    // Between the platform backend's NewFrame and ImGui::NewFrame, like the phone's touches.
    // `connected`: whether any controller is, without which ImGui ignores them.
    void Apply(const std::vector<input::GamepadState>& states, bool connected, ImGuiIO& io);
    // Lets go of whatever is held and hides the controllers from ImGui.
    void Release(ImGuiIO& io);
    // Something is held: ImGui repeats moves and scrolls only while frames go on.
    bool Holding() const;

private:
    void Send(const input::GamepadState& state, ImGuiIO& io);
    void SendKey(size_t index, float value, ImGuiIO& io);
    static void SetConnected(bool connected, ImGuiIO& io);

    std::array<float, kKeyCount> m_sent{};
};

}  // namespace gazeora::app
