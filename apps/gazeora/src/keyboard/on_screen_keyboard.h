#pragma once

#include <array>

#include "gamepad/button_repeat.h"
#include "gamepad/gamepad_presses.h"
#include "keyboard/keyboard_actions.h"
#include "keyboard/keyboard_layout.h"
#include "keyboard/text_sink.h"

namespace gazeora::app {

// A keyboard on screen for typing with a game controller, as in Steam: the d-pad or left
// stick picks a key and the bottom button types it. Shortcuts: left button deletes, top
// button types a space, left shoulder is Shift, right shoulder the symbols, and the right
// button or Start finishes.
class OnScreenKeyboard final : public IKeyboardActions {
public:
    explicit OnScreenKeyboard(ITextSink& sink);

    // Starts on the letters, at Q.
    void Open();
    void Close() { m_open = false; }
    bool IsOpen() const { return m_open; }
    // Once a frame while open, with the controllers' presses.
    void Update(const GamepadPresses& presses, double nowSeconds);

    const KeyboardLayout& Layout() const { return m_symbols ? m_symbolLayout : m_letterLayout; }
    int Row() const { return m_row; }
    int Column() const { return m_column; }

    void Type(char32_t character) override;
    void Erase() override;
    void ToggleShift() override { m_shifted = !m_shifted; }
    void ToggleSymbols() override;
    void Finish() override;
    bool Shifted() const override { return m_shifted; }

private:
    enum Direction { kUp, kDown, kLeft, kRight, kDirectionCount };

    void FollowShortcuts(const GamepadPresses& presses, double nowSeconds);
    void FollowDirections(const input::GamepadState& state, double nowSeconds);
    void Move(Direction direction);
    void MoveAcross(int step);
    void MoveToRow(int row);
    int NearestColumn(int row, float center) const;
    // Where the key at `column` of `row` is centered, in ordinary keys from the left.
    float CenterOf(int row, int column) const;

    ITextSink& m_sink;
    const KeyboardLayout m_letterLayout = KeyboardLayout::Letters();
    const KeyboardLayout m_symbolLayout = KeyboardLayout::Symbols();
    bool m_open = false;
    bool m_symbols = false;
    bool m_shifted = false;
    int m_row = 0;
    int m_column = 0;
    std::array<ButtonRepeat, kDirectionCount> m_moves;
    ButtonRepeat m_erase;
};

}  // namespace gazeora::app
