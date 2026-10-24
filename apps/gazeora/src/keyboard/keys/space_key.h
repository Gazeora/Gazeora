#pragma once

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

class SpaceKey final : public IKeyboardKey {
public:
    static constexpr float kSpan = 4.0f;

    std::string Label(bool) const override { return "Space"; }
    void Press(IKeyboardActions& actions) const override { actions.Type(U' '); }
    float Span() const override { return kSpan; }
};

}  // namespace gazeora::app
