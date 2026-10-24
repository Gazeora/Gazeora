#include "gazeora/input/models/gamepad_state.h"

#include <algorithm>
#include <cmath>

namespace gazeora::input {

namespace {

constexpr float kAxisMin = -1.0f;
constexpr float kAxisMax = 1.0f;

}  // namespace

bool GamepadState::IsNeutral() const {
    return m_buttons.none() &&
           std::all_of(m_axes.begin(), m_axes.end(), [](float value) { return value == 0.0f; });
}

void GamepadState::SetAxis(GamepadAxis axis, float value) {
    m_axes[size_t(axis)] = std::clamp(value, kAxisMin, kAxisMax);
}

void GamepadState::Merge(const GamepadState& other) {
    m_buttons |= other.m_buttons;
    for (size_t i = 0; i < m_axes.size(); ++i) {
        if (std::fabs(other.m_axes[i]) > std::fabs(m_axes[i])) {
            m_axes[i] = other.m_axes[i];
        }
    }
}

bool GamepadState::operator==(const GamepadState& other) const {
    return m_buttons == other.m_buttons && m_axes == other.m_axes;
}

}  // namespace gazeora::input
