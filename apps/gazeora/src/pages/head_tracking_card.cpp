// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/head_tracking_card.h"

#include "head/mouse_look_driver.h"
#include "ui/head_tracking_status_line.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;

}  // namespace

void HeadTrackingCard::Draw(ImFont* heading, ImFont* caption, float width) {
    BeginCard("##headtracking", m_scale, width);
    CardTitle(heading, ICON_CAMERA "  Head tracking");
    DrawSwitch();
    ImGui::BeginDisabled(!m_control.Enabled());
    DrawSensitivity(caption);
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
        "With the phone in a VR viewer, turning your head moves the mouse in the game, the "
        "way mouse-look games turn their camera. Only while a game launched from Gazeora is "
        "the window in front.",
        S(kHelpWidth));
}

void HeadTrackingCard::DrawSensitivity(ImFont* caption) {
    FieldLabel(caption, "Sensitivity");
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
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(
        "Raise it if the game turns less than your head, lower it if it turns more. Each game, "
        "and its own mouse sensitivity, needs its own.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void HeadTrackingCard::DrawStatus() {
    HeadTrackingStatusLine status(m_control.Enabled(), m_control.Receiving(),
                                  m_control.GameInFront());
    ImGui::PushStyleColor(ImGuiCol_Text, status.Color());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.Text());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace gazeora::app
