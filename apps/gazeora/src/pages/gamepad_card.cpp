#include "pages/gamepad_card.h"

#include "ui/gamepad_status_line.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;

// Buttons by where they sit, as every make reads them; each make's own names in brackets.
constexpr const char* kGuide[] = {
    "Move: the d-pad or the left stick",
    "Select: the bottom face button (A on Xbox, Cross on PlayStation, B on Nintendo)",
    "Back: the right face button (B, Circle, A): closes a list or leaves a field",
    "Pages: the shoulder buttons (LB and RB, L1 and R1, L and R)",
    "Scroll: the right stick",
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
        "Any controller works: Xbox, PlayStation, Nintendo Switch Pro and most others, "
        "plugged in or over Bluetooth. Several at once work too.\n\nIt drives Gazeora while "
        "its window is in front, or while the stream shows it, as in the headset. While a "
        "game launched from here runs, Gazeora lets go of the controller and leaves it to "
        "the game.",
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
