// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/update_card.h"

#include <string>

#include "platform/shell.h"
#include "ui/theme.h"
#include "ui/update_status_line.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;
// ImGui animates a progress bar given a negative fraction that changes over time.
constexpr float kIndeterminateSpeed = -1.0f;

}  // namespace

UpdateCard::UpdateCard(UpdateControl& control, float scale) : m_control(control), m_scale(scale) {}

void UpdateCard::Draw(ImFont* heading, float width, bool gameRunning) {
    BeginCard("##updates", m_scale, width);
    CardTitle(heading, ICON_REFRESH "  Updates");
    DrawSwitch();
    DrawStatus();
    DrawActions(gameRunning);
    EndCard();
}

void UpdateCard::DrawSwitch() {
    ImGui::AlignTextToFramePadding();
    bool check = m_control.CheckOnStart();
    if (Toggle("##updatecheck", &check, m_scale)) {
        m_control.SetCheckOnStart(check);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Check for updates when Gazeora starts");
    HelpMarker(
        "Gazeora reads the list of releases from gazeora.github.io. Nothing is downloaded "
        "or installed until you choose to. A release candidate is offered newer release "
        "candidates too; a release only newer releases.",
        S(kHelpWidth));
}

void UpdateCard::DrawStatus() {
    UpdateStatusLine status(m_control);
    ImGui::PushStyleColor(ImGuiCol_Text, status.Color());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.Text().c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (m_control.State() == UpdateState::Downloading) {
        const float progress = m_control.Progress();
        ImGui::ProgressBar(progress < 0.0f ? kIndeterminateSpeed * float(ImGui::GetTime())
                                           : progress);
    }
}

void UpdateCard::DrawActions(bool gameRunning) {
    switch (m_control.State()) {
        case UpdateState::Idle:
        case UpdateState::UpToDate:
            if (ImGui::Button(ICON_REFRESH "  Check now")) {
                m_control.Check();
            }
            return;
        case UpdateState::Available:
            DrawInstallButtons(gameRunning);
            return;
        case UpdateState::Failed:
            if (ImGui::Button(ICON_REFRESH "  Try again")) {
                m_control.Retry();
            }
            DrawReleasePageButton("  Download it yourself");
            return;
        case UpdateState::Checking:
        case UpdateState::Downloading:
        case UpdateState::Installing:
            return;
    }
}

void UpdateCard::DrawInstallButtons(bool gameRunning) {
    ImGui::BeginDisabled(gameRunning);
    ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
    if (ImGui::Button(ICON_PLAY "  Download and install")) {
        m_control.Install();
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", gameRunning
                                    ? "Close the game first: Gazeora closes while it updates."
                                    : "Gazeora closes while the installer runs. Your library "
                                      "and settings are kept.");
    }
    DrawReleasePageButton("  What's new");
}

void UpdateCard::DrawReleasePageButton(const char* label) {
    if (!m_control.Update()) {
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(ICON_INFO) + label).c_str())) {
        OpenUrl(m_control.Update()->PageUrl());
    }
}

}  // namespace gazeora::app
