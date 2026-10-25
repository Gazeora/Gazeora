#include "pages/gamepad_card.h"

#include "ui/gamepad_status_line.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;

// Buttons by where they sit, as every make reads them; each make's own names in brackets.
constexpr const char* kGuide[] = {
    "Move: d-pad or left stick. Left and right switch between the menu and the page",
    "Select: bottom face button (A on Xbox, Cross on PlayStation, B on Nintendo)",
    "Back: right face button (B, Circle, A). Closes what's open or goes back a step",
    "Game options: Menu or Options button on a game: play, details, remove",
    "Switch pages: shoulder buttons (LB and RB, L1 and R1, L and R)",
    "Scroll: right stick",
    "Type: select a text box to bring up the on-screen keyboard",
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
        "Works with Xbox, PlayStation, Nintendo Switch Pro and most other controllers, "
        "wired or over Bluetooth. You can use several at once.\n\nThe controller works "
        "Gazeora while its window is in front or showing on the stream, such as in the "
        "headset. While a game you launched from here runs, Gazeora leaves the controller "
        "to the game.",
        S(kHelpWidth));
}

void GamepadCard::DrawGuide() {
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    for (const char* line : kGuide) {
        ImGui::TextUnformatted(line);
    }
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
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
