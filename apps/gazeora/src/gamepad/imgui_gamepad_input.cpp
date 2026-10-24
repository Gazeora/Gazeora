#include "gamepad/imgui_gamepad_input.h"

#include <algorithm>
#include <iterator>

namespace gazeora::app {

namespace {

using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadState;

// A stick standing in for the d-pad presses past half way, and lets go only further back,
// so one held near the edge doesn't step twice.
constexpr float kPressPush = 0.5f;
constexpr float kReleasePush = 0.35f;
constexpr float kReleased = 0.0f;
constexpr float kPressed = 1.0f;

// How far a key is pushed, from 0 to 1.
using PushReader = float (*)(const GamepadState&);

template <GamepadButton button>
float Button(const GamepadState& state) {
    return state.IsDown(button) ? kPressed : kReleased;
}

template <GamepadAxis axis, int sign>
float Stick(const GamepadState& state) {
    return std::max(kReleased, float(sign) * state.Axis(axis));
}

template <GamepadButton button, GamepadAxis axis, int sign>
float DpadOrStick(const GamepadState& state) {
    return std::max(Button<button>(state), Stick<axis, sign>(state));
}

struct KeySource {
    ImGuiKey key;
    PushReader push;
    bool analog;  // sent as it is; otherwise pressed or not
};

// Only what ImGui's navigation uses; the other buttons are the launcher's own (GamepadPresses).
// The left face button would open ImGui's window switcher, which this UI has no use for.
constexpr KeySource kKeys[] = {
    {ImGuiKey_GamepadFaceDown, Button<GamepadButton::South>, false},
    {ImGuiKey_GamepadFaceRight, Button<GamepadButton::East>, false},
    {ImGuiKey_GamepadDpadUp, DpadOrStick<GamepadButton::DpadUp, GamepadAxis::LeftY, -1>, false},
    {ImGuiKey_GamepadDpadDown, DpadOrStick<GamepadButton::DpadDown, GamepadAxis::LeftY, 1>, false},
    {ImGuiKey_GamepadDpadLeft, DpadOrStick<GamepadButton::DpadLeft, GamepadAxis::LeftX, -1>, false},
    {ImGuiKey_GamepadDpadRight, DpadOrStick<GamepadButton::DpadRight, GamepadAxis::LeftX, 1>,
     false},
    {ImGuiKey_GamepadRStickUp, Stick<GamepadAxis::RightY, -1>, true},
    {ImGuiKey_GamepadRStickDown, Stick<GamepadAxis::RightY, 1>, true},
    {ImGuiKey_GamepadRStickLeft, Stick<GamepadAxis::RightX, -1>, true},
    {ImGuiKey_GamepadRStickRight, Stick<GamepadAxis::RightX, 1>, true},
};
static_assert(std::size(kKeys) == ImGuiGamepadInput::kKeyCount, "one sent value per key");

float Pressed(float push, float sent) {
    float threshold = sent > kReleased ? kReleasePush : kPressPush;
    return push > threshold ? kPressed : kReleased;
}

}  // namespace

void ImGuiGamepadInput::Apply(const std::vector<GamepadState>& states, bool connected,
                              ImGuiIO& io) {
    SetConnected(connected, io);
    for (const GamepadState& state : states) {
        Send(state, io);
    }
}

void ImGuiGamepadInput::Release(ImGuiIO& io) {
    Send(GamepadState(), io);
    SetConnected(false, io);
}

bool ImGuiGamepadInput::Holding() const {
    return std::any_of(m_sent.begin(), m_sent.end(), [](float value) { return value > kReleased; });
}

void ImGuiGamepadInput::Send(const GamepadState& state, ImGuiIO& io) {
    for (size_t i = 0; i < kKeyCount; ++i) {
        float push = kKeys[i].push(state);
        SendKey(i, kKeys[i].analog ? push : Pressed(push, m_sent[i]), io);
    }
}

void ImGuiGamepadInput::SendKey(size_t index, float value, ImGuiIO& io) {
    if (value == m_sent[index]) {
        return;
    }
    m_sent[index] = value;
    io.AddKeyAnalogEvent(kKeys[index].key, value > kReleased, value);
}

void ImGuiGamepadInput::SetConnected(bool connected, ImGuiIO& io) {
    if (connected) {
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    } else {
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    }
}

}  // namespace gazeora::app
