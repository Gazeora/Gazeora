#pragma once

namespace gazeora::app {

// The right stick scrolls the window being drawn: gently near the center, quickly pushed all
// the way. ImGui's own stick scrolling moves only the window holding the focus, which is
// often a card that can't scroll.
class GamepadScroll {
public:
    explicit GamepadScroll(float scale) : m_scale(scale) {}

    // Call inside the window to scroll, once a frame.
    void Apply() const;

private:
    float m_scale;
};

}  // namespace gazeora::app
