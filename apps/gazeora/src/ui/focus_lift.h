#pragma once

namespace gazeora::app {

// How the control a controller is on stands out, as in Steam: lifted a little, with a lighter
// surface and a faint glow fading out beneath it. In place of ImGui's box, which ran over the
// controls next to it.
class FocusLift {
public:
    explicit FocusLift(float scale) : m_scale(scale) {}

    // Once a frame, after everything is drawn and before ImGui::Render.
    void Draw() const;

private:
    float S(float px) const { return px * m_scale; }

    float m_scale;
};

}  // namespace gazeora::app
