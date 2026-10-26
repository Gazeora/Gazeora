#include "pages/controls_page.h"

#include <algorithm>

#include "ui/theme.h"

namespace gazeora::app {

namespace {

constexpr float kMaxCardWidth = 720.0f;
constexpr float kTitleGap = 6.0f;
constexpr ImVec2 kCardItemSpacing(10.0f, 14.0f);

}  // namespace

void ControlsPage::Draw(const Fonts& fonts) {
    ImGui::PushFont(fonts.title);
    ImGui::TextUnformatted("Controls");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted("Your mouse and keyboard always work. Add other ways to play here.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, S(kTitleGap)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(S(kCardItemSpacing.x), S(kCardItemSpacing.y)));
    float width = std::min(ImGui::GetContentRegionAvail().x, S(kMaxCardWidth));
    m_gamepad.Draw(fonts.heading, width);
    ImGui::PopStyleVar();
}

}  // namespace gazeora::app
