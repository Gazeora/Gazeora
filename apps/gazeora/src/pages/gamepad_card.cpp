#include "pages/gamepad_card.h"

#include "ui/gamepad_status_line.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;

struct GuideRow {
    const char* action;
    const char* buttons;
};

// Buttons by where they sit, as every make reads them.
constexpr GuideRow kGuide[] = {
    {"Move", "D-pad or left stick"},
    {"Select", "Bottom button (A, Cross)"},
    {"Back", "Right button (B, Circle)"},
    {"Game options", "Menu button on a game"},
    {"Switch pages", "Shoulder buttons"},
    {"Scroll", "Right stick"},
    {"Type", "Select a text box"},
};

}  // namespace

GamepadCard::GamepadCard(GamepadControl& control, float scale)
    : m_control(control), m_scale(scale) {}

void GamepadCard::Draw(ImFont* heading, float width) {
    BeginCard("##gamepad", m_scale, width);
    CardTitle(heading, ICON_GAME "  Game controller");
    DrawSwitch();
    ImGui::BeginDisabled(!m_control.Enabled());
    DrawGuide();
    ImGui::EndDisabled();
    DrawStatus();
    EndCard();
}

void GamepadCard::DrawSwitch() {
    ImGui::AlignTextToFramePadding();
    bool enabled = m_control.Enabled();
    if (Toggle("##gamepadon", &enabled, m_scale)) {
        m_control.SetEnabled(enabled);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Move around Gazeora with a game controller");
    HelpMarker(
        "Works with Xbox, PlayStation, Switch Pro and most other controllers, wired or "
        "Bluetooth. It works while Gazeora is in front or on the stream, and is left to "
        "the game while one runs.",
        S(kHelpWidth));
}

void GamepadCard::DrawGuide() {
    if (!ImGui::BeginTable("##guide", 2, ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    for (const GuideRow& row : kGuide) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kTextDim), "%s", row.action);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.buttons);
    }
    ImGui::EndTable();
}

void GamepadCard::DrawStatus() {
    input::GamepadSnapshot snapshot = m_control.Snapshot();
    GamepadStatusLine status(snapshot, m_control.Paused());
    ImGui::PushStyleColor(ImGuiCol_Text, status.Color());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.Text().c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (snapshot.state == input::GamepadSessionState::Error &&
        ImGui::Button(ICON_REFRESH "  Try again")) {
        m_control.Restart();
    }
}

}  // namespace gazeora::app
