// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/imgui_gamepad_input.h"

#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <string>
#include <vector>

namespace gazeora::app {
namespace {

using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadState;

constexpr float kFrameSeconds = 1.0f / 60.0f;
// ImGui takes a key's presses and releases a frame at a time; this is enough for any test.
constexpr int kSettleFrames = 4;

// A real ImGui context, headless: a sidebar and a page side by side, flattened into one
// window as the launcher's are, and what was focused and pressed in the last frames.
class ImGuiGamepadInputTest : public ::testing::Test {
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
    }

    void TearDown() override { ImGui::DestroyContext(m_context); }

    void Apply(const GamepadState& state) {
        m_input.Apply({state}, true, ImGui::GetIO());
        Run(kSettleFrames);
    }

    // Presses `button` and lets go of it again.
    void Tap(GamepadButton button) {
        GamepadState pressed;
        pressed.SetButton(button, true);
        Apply(pressed);
        Apply(GamepadState());
    }

    // The first move only lands focus somewhere in the window (ImGui moves from its top edge);
    // up from there is the sidebar's first entry, as in the launcher.
    void FocusFirst() {
        Run(kSettleFrames);
        Tap(GamepadButton::DpadDown);
        Tap(GamepadButton::DpadUp);
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
        Pane("##sidebar", {"Home", "Library"});
        ImGui::SameLine();
        Pane("##page", {"Play", "Details"});
        ImGui::End();
        ImGui::Render();
    }

    void Pane(const char* id, const std::vector<std::string>& buttons) {
        ImGui::BeginChild(id, ImVec2(200, 0), ImGuiChildFlags_NavFlattened);
        for (const std::string& label : buttons) {
            if (ImGui::Button(label.c_str())) {
                m_pressed.push_back(label);
            }
            if (ImGui::IsItemFocused()) {
                m_focused = label;
            }
        }
        ImGui::EndChild();
    }

    ImGuiContext* m_context = nullptr;
    ImGuiGamepadInput m_input;
    std::string m_focused;
    std::vector<std::string> m_pressed;
};

TEST_F(ImGuiGamepadInputTest, TellsImGuiWhetherAControllerIsConnected) {
    ImGuiIO& io = ImGui::GetIO();
    m_input.Apply({}, true, io);
    EXPECT_TRUE(io.BackendFlags & ImGuiBackendFlags_HasGamepad);
    m_input.Release(io);
    EXPECT_FALSE(io.BackendFlags & ImGuiBackendFlags_HasGamepad);
}

TEST_F(ImGuiGamepadInputTest, MovesFromTheSidebarIntoThePage) {
    FocusFirst();
    EXPECT_EQ(m_focused, "Home");
    Tap(GamepadButton::DpadRight);
    EXPECT_EQ(m_focused, "Play");
    Tap(GamepadButton::DpadDown);
    EXPECT_EQ(m_focused, "Details");
}

TEST_F(ImGuiGamepadInputTest, TheBottomButtonPresses) {
    FocusFirst();
    Tap(GamepadButton::South);
    ASSERT_EQ(m_pressed.size(), 1u);
    EXPECT_EQ(m_pressed.front(), "Home");
}

TEST_F(ImGuiGamepadInputTest, TheLeftStickMovesLikeTheDpad) {
    FocusFirst();
    GamepadState right;
    right.SetAxis(GamepadAxis::LeftX, 0.9f);
    Apply(right);
    Apply(GamepadState());
    EXPECT_EQ(m_focused, "Play");
}

TEST_F(ImGuiGamepadInputTest, AStickNearTheEdgeStaysPressed) {
    GamepadState stick;
    stick.SetAxis(GamepadAxis::LeftY, 0.55f);
    Apply(stick);
    EXPECT_TRUE(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown));
    stick.SetAxis(GamepadAxis::LeftY, 0.45f);
    Apply(stick);
    EXPECT_TRUE(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown));
    stick.SetAxis(GamepadAxis::LeftY, 0.3f);
    Apply(stick);
    EXPECT_FALSE(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown));
}

TEST_F(ImGuiGamepadInputTest, TheRightStickGoesThroughAsItIsPushed) {
    GamepadState stick;
    stick.SetAxis(GamepadAxis::RightY, 0.5f);
    Apply(stick);
    EXPECT_FLOAT_EQ(ImGui::GetKeyData(ImGuiKey_GamepadRStickDown)->AnalogValue, 0.5f);
    EXPECT_FLOAT_EQ(ImGui::GetKeyData(ImGuiKey_GamepadRStickUp)->AnalogValue, 0.0f);
    EXPECT_TRUE(m_input.Holding());
}

TEST_F(ImGuiGamepadInputTest, ReleaseLetsGoOfEverythingHeld) {
    GamepadState held;
    held.SetButton(GamepadButton::South, true);
    Apply(held);
    EXPECT_TRUE(ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
    m_input.Release(ImGui::GetIO());
    Run(kSettleFrames);
    EXPECT_FALSE(ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
    EXPECT_FALSE(m_input.Holding());
}

}  // namespace
}  // namespace gazeora::app
