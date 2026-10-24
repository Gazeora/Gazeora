#include "ui/focus_lift.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "ui/theme.h"

namespace gazeora::app {

namespace {

constexpr float kRounding = 6.0f;       // the controls' own (style.FrameRounding)
constexpr int kGlowSteps = 4;           // one pixel each, fading out
constexpr float kGlowAlpha = 0.30f;     // nearest the control: visible on a phone in a headset too
constexpr float kDrop = 1.5f;           // the glow sits a little low, as under a raised card
constexpr float kSurfaceAlpha = 0.08f;  // the control lightens this much

// ImGui's own back steps out onto a whole pane for a frame: that isn't a control to lift.
bool OnPane(const ImGuiWindow* window) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    for (const ImGuiWindow* child : window->DC.ChildWindows) {
        if (child->ChildId == g.NavId) {
            return true;
        }
    }
    return false;
}

}  // namespace

void FocusLift::Draw() const {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.NavWindow;
    if (!g.NavCursorVisible || g.NavId == 0 || !g.NavIdIsAlive || !window || OnPane(window)) {
        return;
    }
    // Into the control's own window, so popups and cards above it stay above it.
    ImRect rect = ImGui::WindowRectRelToAbs(window, window->NavRectRel[g.NavLayer]);
    ImDrawList* draw = window->DrawList;
    draw->PushClipRect(window->ClipRect.Min, window->ClipRect.Max);
    float rounding = S(kRounding);
    for (int i = 1; i <= kGlowSteps; ++i) {
        float spread = S(float(i));
        float fade = 1.0f - float(i - 1) / float(kGlowSteps);
        ImVec2 drop(0.0f, S(kDrop));
        draw->AddRect(rect.Min - ImVec2(spread, spread) + drop,
                      rect.Max + ImVec2(spread, spread) + drop,
                      WithAlpha(kAccent, kGlowAlpha * fade), rounding + spread, 0, S(1.0f));
    }
    draw->AddRectFilled(rect.Min, rect.Max, WithAlpha(kText, kSurfaceAlpha), rounding);
    draw->PopClipRect();
}

}  // namespace gazeora::app
