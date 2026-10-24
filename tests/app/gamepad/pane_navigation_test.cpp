#include "gamepad/pane_navigation.h"

#include <gtest/gtest.h>
#include <imgui.h>

#include <string>

#include "gamepad/imgui_gamepad_input.h"

namespace gazeora::app {
namespace {

using input::GamepadButton;
using input::GamepadState;

constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr int kSettleFrames = 4;
// The page's controls sit well below the sidebar's, as in the launcher: ImGui alone never
// finds them going right.
constexpr float kPageTopGap = 200.0f;

// A headless ImGui laid out as the launcher is: a sidebar and a page, regions of their own.
class PaneNavigationTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = kFrameSeconds;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Run(kSettleFrames);
    }

    void TearDown() override { ImGui::DestroyContext(m_context); }

    void Tap(GamepadButton button) {
        GamepadState pressed;
        pressed.SetButton(button, true);
        m_input.Apply({pressed}, true, ImGui::GetIO());
        Run(kSettleFrames);
        m_input.Apply({GamepadState()}, true, ImGui::GetIO());
        Run(kSettleFrames);
    }

    void Run(int frames) {
        for (int i = 0; i < frames; ++i) {
            Frame();
        }
    }

    void Frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration);
        m_panes.Apply();
        ImGui::BeginChild("##sidebar", ImVec2(200, 0));
        m_panes.MarkSidebar();
        Buttons({"Home", "Library"});
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##page", ImVec2(0, 0));
        m_panes.MarkPage();
        ImGui::Dummy(ImVec2(0, kPageTopGap));
        Buttons({"Play", "Details"});
        ImGui::EndChild();
        ImGui::End();
        m_panes.Follow();
        ImGui::Render();
    }

    void Buttons(std::initializer_list<const char*> labels) {
        for (const char* label : labels) {
            ImGui::Button(label);
            if (label == m_claimOnEntry) {
                m_panes.FocusHereOnEntry();
            }
            if (ImGui::IsItemFocused()) {
                m_focused = label;
            }
        }
    }

    ImGuiContext* m_context = nullptr;
    ImGuiGamepadInput m_input;
    PaneNavigation m_panes;
    std::string m_focused;
    std::string m_claimOnEntry;
};

TEST_F(PaneNavigationTest, StartsInThePage) { EXPECT_EQ(m_focused, "Play"); }

TEST_F(PaneNavigationTest, LeftAndRightCrossBetweenTheRegions) {
    Tap(GamepadButton::DpadDown);
    EXPECT_EQ(m_focused, "Details");
    Tap(GamepadButton::DpadLeft);
    EXPECT_EQ(m_focused, "Home");
    Tap(GamepadButton::DpadDown);
    EXPECT_EQ(m_focused, "Library");
    Tap(GamepadButton::DpadRight);
    EXPECT_EQ(m_focused, "Details");  // back where it was
}

TEST_F(PaneNavigationTest, AnEnteredPageStartsAtItsFirstControl) {
    Tap(GamepadButton::DpadDown);
    m_panes.EnterPage();
    Run(kSettleFrames);
    EXPECT_EQ(m_focused, "Play");
}

TEST_F(PaneNavigationTest, AControlCanClaimFocusOnEntry) {
    m_claimOnEntry = "Details";
    m_panes.EnterPage();
    Run(kSettleFrames);
    EXPECT_EQ(m_focused, "Details");
}

TEST_F(PaneNavigationTest, BackToTheSidebarLandsOnTheOpenPage) {
    m_claimOnEntry = "Library";  // the open page's entry
    m_panes.ReturnToSidebar();
    Run(kSettleFrames);
    EXPECT_EQ(m_focused, "Library");
    EXPECT_TRUE(m_panes.InSidebar());
}

TEST_F(PaneNavigationTest, AChangedPageIsEnteredAtItsTop) {
    Tap(GamepadButton::DpadDown);
    EXPECT_EQ(m_focused, "Details");
    Tap(GamepadButton::DpadLeft);
    m_panes.PageChanged();
    Tap(GamepadButton::DpadRight);
    EXPECT_EQ(m_focused, "Play");
}

TEST_F(PaneNavigationTest, SettlesOnceFocusHasLanded) {
    m_panes.EnterPage();
    EXPECT_TRUE(m_panes.Settling());
    Run(kSettleFrames);
    EXPECT_FALSE(m_panes.Settling());
}

}  // namespace
}  // namespace gazeora::app
