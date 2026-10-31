// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "ui/focus_edge.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

#include "ui/theme.h"

namespace gazeora::app {

namespace {

constexpr float kRounding = 6.0f;   // the controls' own (style.FrameRounding)
constexpr float kThickness = 2.0f;  // the gradient edge
constexpr float kSeparator = 1.0f;  // a dark line inside it, so it shows on gradient buttons too
constexpr float kSeparatorAlpha = 0.7f;

// ImGui's own back steps out onto a whole pane for a frame: that isn't a control to mark.
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

void FocusEdge::Draw() const {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.NavWindow;
    if (!g.NavCursorVisible || g.NavId == 0 || !g.NavIdIsAlive || !window || OnPane(window)) {
        return;
    }
    // Into the control's own window, so popups and cards above it stay above it.
    ImRect rect = ImGui::WindowRectRelToAbs(window, window->NavRectRel[g.NavLayer]);
    ImDrawList* draw = window->DrawList;
    draw->PushClipRect(window->ClipRect.Min, window->ClipRect.Max);
    float rounding = std::min(S(kRounding), rect.GetHeight() * 0.5f);
    float edge = S(kThickness);
    float separator = S(kSeparator);

    ImVec2 inset(edge * 0.5f, edge * 0.5f);
    int first = draw->VtxBuffer.Size;
    draw->AddRect(rect.Min + inset, rect.Max - inset, IM_COL32_WHITE, rounding, 0, edge);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, first, draw->VtxBuffer.Size, rect.Min,
                                                  ImVec2(rect.Max.x, rect.Min.y), kAccent,
                                                  kAccentEnd);

    ImVec2 inner(edge + separator * 0.5f, edge + separator * 0.5f);
    draw->AddRect(rect.Min + inner, rect.Max - inner, WithAlpha(kWindowBg, kSeparatorAlpha),
                  std::max(0.0f, rounding - edge), 0, separator);
    draw->PopClipRect();
}

}  // namespace gazeora::app
