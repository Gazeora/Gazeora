#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// Keeps the text and closes the keyboard.
class DoneKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 2.0f;

    std::string Label(bool) const override { return "Done"; }
    void Press(IKeyboardActions& actions) const override { actions.Finish(); }
    float Span() const override { return kSpan; }
};

}  // namespace gazeora::app
