// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/head_tracking_card.h"

#include "gazeora/common/shared.h"
#include "head/controller_look_driver.h"
#include "head/mouse_look_driver.h"
#include "ui/head_tracking_status_line.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;
constexpr float kPercent = 100.0f;

// A dim, wrapped line explaining the controls above it.
void Hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace

void HeadTrackingCard::Draw(ImFont* heading, ImFont* caption, float width) {
    BeginCard("##headtracking", m_scale, width);
    CardTitle(heading, ICON_CAMERA "  Head tracking");
    DrawSwitch();
    ImGui::BeginDisabled(!m_control.Enabled());
    DrawSensitivity(caption);
    DrawController(caption);
    ImGui::EndDisabled();
    DrawStatus();
    EndCard();
}

void HeadTrackingCard::DrawSwitch() {
    ImGui::AlignTextToFramePadding();
    bool enabled = m_control.Enabled();
    if (Toggle("##headon", &enabled, m_scale)) {
        m_control.SetEnabled(enabled);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Turn the game's camera with your head");
    HelpMarker(
        "With the phone in a VR viewer, turning your head turns the game's camera, through "
        "whichever you're playing with: the mouse, or the controller's right stick, so the game "
        "never switches to the other. Only while a game launched from Gazeora is the window in "
        "front.",
        S(kHelpWidth));
}

void HeadTrackingCard::DrawSensitivity(ImFont* caption) {
    FieldLabel(caption, "Mouse sensitivity");
    float counts = m_control.CountsPerDegree();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##headcounts", &counts, MouseLookDriver::kMinCountsPerDegree,
                           MouseLookDriver::kMaxCountsPerDegree, "%.0f mouse counts per degree",
                           ImGuiSliderFlags_Logarithmic)) {
        m_control.SetCountsPerDegree(counts);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_control.SaveCountsPerDegree();
    }
    Hint(
        "Raise it if the game turns less than your head, lower it if it turns more. Each game, "
        "and its own mouse sensitivity, needs its own.");
}

void HeadTrackingCard::DrawController(ImFont* caption) {
    float degrees = m_control.StickDegreesPerSecond();
    float deadZone = m_control.StickDeadZone() * kPercent;
    FieldLabel(caption, "Controller: the game's turn at full stick");
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::SliderFloat(
        "##stickspeed", &degrees, ControllerLookDriver::kMinDegreesPerSecond,
        ControllerLookDriver::kMaxDegreesPerSecond, "%.0f° a second", ImGuiSliderFlags_Logarithmic);
    bool released = ImGui::IsItemDeactivatedAfterEdit();
    FieldLabel(caption, "Controller: the game's stick dead zone");
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::SliderFloat("##stickdeadzone", &deadZone, 0.0f,
                                  ControllerLookDriver::kMaxDeadZone * kPercent, "%.0f%%");
    released |= ImGui::IsItemDeactivatedAfterEdit();
    if (changed) {
        m_control.SetStick(degrees, deadZone / kPercent);
    }
    if (released) {
        m_control.SaveStick();
    }
    Hint(
        "A stick sets how fast the camera turns. If the game turns less than your head, lower "
        "the turn; more, raise it. If small head turns go missing, raise the dead zone; if the "
        "camera twitches, lower it.");
    DrawControllerSeen();
}

void HeadTrackingCard::DrawControllerSeen() {
    const LONG paths = m_control.ControllerPaths();
    const char* seen = "The game hasn't read a controller yet";
    if (paths & (kPathSonyRead | kPathSonyOverlapped | kPathSonyGetReport)) {
        seen = "The game reads a PlayStation controller on USB";
    } else if (paths & kPathXInput) {
        seen = "The game reads an Xbox-style controller (XInput)";
    }
    Hint(seen);
}

void HeadTrackingCard::DrawStatus() {
    HeadTrackingStatusLine::State state;
    state.enabled = m_control.Enabled();
    state.receiving = m_control.Receiving();
    state.gameInFront = m_control.GameInFront();
    state.onController = m_control.OnController();
    HeadTrackingStatusLine status(state);
    ImGui::PushStyleColor(ImGuiCol_Text, status.Color());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.Text());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace gazeora::app
