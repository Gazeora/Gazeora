#include "keyboard/on_screen_keyboard.h"

#include <cmath>

namespace gazeora::app {

namespace {

using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadState;

// Where it starts: the letters' second row, at Q.
constexpr int kStartRow = 1;
// A stick past half way moves like the d-pad.
constexpr float kStickPush = 0.5f;

struct DirectionInput {
    GamepadButton dpad;
    GamepadAxis axis;
    float sign;
};

// In Direction's order: up, down, left, right.
constexpr DirectionInput kDirections[] = {
    {GamepadButton::DpadUp, GamepadAxis::LeftY, -1.0f},
    {GamepadButton::DpadDown, GamepadAxis::LeftY, 1.0f},
    {GamepadButton::DpadLeft, GamepadAxis::LeftX, -1.0f},
    {GamepadButton::DpadRight, GamepadAxis::LeftX, 1.0f},
};

bool Held(const GamepadState& state, const DirectionInput& input) {
    return state.IsDown(input.dpad) || input.sign * state.Axis(input.axis) > kStickPush;
}

}  // namespace

OnScreenKeyboard::OnScreenKeyboard(ITextSink& sink) : m_sink(sink) {}

void OnScreenKeyboard::Open() {
    m_open = true;
    m_symbols = false;
    m_shifted = false;
    m_row = kStartRow;
    m_column = 0;
}

void OnScreenKeyboard::Update(const GamepadPresses& presses, double nowSeconds) {
    if (!m_open) {
        return;
    }
    FollowDirections(presses.Current(), nowSeconds);
    FollowShortcuts(presses, nowSeconds);
}

void OnScreenKeyboard::FollowShortcuts(const GamepadPresses& presses, double nowSeconds) {
    if (presses.Pressed(GamepadButton::South)) {
        Layout().Key(m_row, m_column).Press(*this);
        if (!m_open) {
            return;  // it was Done
        }
    }
    if (m_erase.Fire(presses.Current().IsDown(GamepadButton::West), nowSeconds)) {
        Erase();
    }
    if (presses.Pressed(GamepadButton::North)) {
        Type(U' ');
    }
    if (presses.Pressed(GamepadButton::LeftShoulder)) {
        ToggleShift();
    }
    if (presses.Pressed(GamepadButton::RightShoulder)) {
        ToggleSymbols();
    }
    if (presses.Pressed(GamepadButton::East) || presses.Pressed(GamepadButton::Start)) {
        Finish();
    }
}

void OnScreenKeyboard::FollowDirections(const GamepadState& state, double nowSeconds) {
    for (int i = 0; i < kDirectionCount; ++i) {
        if (m_moves[i].Fire(Held(state, kDirections[i]), nowSeconds)) {
            Move(Direction(i));
        }
    }
}

void OnScreenKeyboard::Type(char32_t character) {
    m_sink.Type(character);
    m_shifted = false;  // like a phone's: Shift is for one character
}

void OnScreenKeyboard::Erase() { m_sink.Erase(); }

void OnScreenKeyboard::ToggleSymbols() {
    float center = CenterOf(m_row, m_column);
    m_symbols = !m_symbols;
    m_shifted = false;
    m_column = NearestColumn(m_row, center);
}

void OnScreenKeyboard::Finish() {
    m_sink.Finish();
    Close();
}

void OnScreenKeyboard::Move(Direction direction) {
    switch (direction) {
        case kUp:
            MoveToRow((m_row + Layout().RowCount() - 1) % Layout().RowCount());
            return;
        case kDown:
            MoveToRow((m_row + 1) % Layout().RowCount());
            return;
        case kLeft:
            MoveAcross(-1);
            return;
        case kRight:
            MoveAcross(1);
            return;
        case kDirectionCount:
            return;
    }
}

void OnScreenKeyboard::MoveAcross(int step) {
    int count = Layout().ColumnCount(m_row);
    m_column = (m_column + step + count) % count;
}

void OnScreenKeyboard::MoveToRow(int row) {
    m_column = NearestColumn(row, CenterOf(m_row, m_column));
    m_row = row;
}

int OnScreenKeyboard::NearestColumn(int row, float center) const {
    // Keys differ in width: the one straight below or above, as near as can be.
    int nearest = 0;
    for (int column = 1; column < Layout().ColumnCount(row); ++column) {
        if (std::fabs(CenterOf(row, column) - center) <
            std::fabs(CenterOf(row, nearest) - center)) {
            nearest = column;
        }
    }
    return nearest;
}

float OnScreenKeyboard::CenterOf(int row, int column) const {
    float left = 0.0f;
    for (int i = 0; i < column; ++i) {
        left += Layout().Key(row, i).Span();
    }
    return left + Layout().Key(row, column).Span() * 0.5f;
}

}  // namespace gazeora::app
