// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/imgui_ui_renderer.h"

#include <imgui.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "gazeora/plugin_ui/parsing/ui_xml_parser.h"
#include "support/fake_ui_values.h"

namespace gazeora::app {
namespace {

constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr float kScale = 1.0f;
constexpr int kSettleFrames = 3;
// The first element's middle: the window's top left corner, past ImGui's default padding.
constexpr ImVec2 kFirstElement(20.0f, 18.0f);

// A plugin's layout drawn on a headless ImGui, as a card on a page draws it.
class ImGuiUiRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = kFrameSeconds;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    void TearDown() override { ImGui::DestroyContext(m_context); }

    void Frame(const ui::UiLayout& layout) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration);
        const UiDrawContext context{values, m_inputs, nullptr, kScale};
        ImGuiUiRenderer(context).DrawAll(layout.children);
        ImGui::End();
        ImGui::Render();
    }

    // Presses and lets go of the mouse at `point`, a frame each, once the layout is on screen:
    // ImGui finds what's under the mouse from the frame before.
    void Click(const ui::UiLayout& layout, ImVec2 point) {
        for (int i = 0; i < kSettleFrames; ++i) {
            Frame(layout);
        }
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(point.x, point.y);
        Frame(layout);
        io.AddMouseButtonEvent(0, true);
        Frame(layout);
        io.AddMouseButtonEvent(0, false);
        Frame(layout);
    }

    bool Read(const std::string& key) const {
        return std::find(values.read.begin(), values.read.end(), key) != values.read.end();
    }

    static ui::UiLayout Layout(const std::string& inside) {
        return ui::UiXmlParser::Parse(R"(<panel page="controls" title="Test">)" + inside +
                                      "</panel>");
    }

    ui::testing::FakeUiValues values;

private:
    ImGuiContext* m_context = nullptr;
    UiInputBuffers m_inputs;
};

TEST_F(ImGuiUiRendererTest, DrawsEveryElementWithItsValue) {
    const ui::UiLayout layout = Layout(R"(
        <text bind="status" tone-bind="tone"/>
        <toggle bind="enabled" label="On" help="Help"/>
        <slider bind="speed" label="Speed" min="0" max="10"/>
        <input bind="name" label="Name" hint="Name"/>
        <choice bind="mode"><option value="a">A</option></choice>
        <group layout="row"><button action="x">X</button><button action="y" icon="add"/></group>
        <table><row><cell>A</cell><cell tone="dim">B</cell></row></table>
        <separator/>)");
    values.values["tone"] = "warning";
    values.values["speed"] = "4.5";
    for (int i = 0; i < 3; ++i) {
        Frame(layout);
    }
    for (const char* key : {"status", "tone", "enabled", "speed", "name", "mode"}) {
        EXPECT_TRUE(Read(key)) << key;
    }
    EXPECT_TRUE(values.actions.empty());
}

TEST_F(ImGuiUiRendererTest, LeavesOutWhatIsHidden) {
    const ui::UiLayout layout = Layout(R"(<text visible-if="failed" bind="why"/>)");
    Frame(layout);
    EXPECT_TRUE(Read("failed"));
    EXPECT_FALSE(Read("why"));
    values.values["failed"] = "1";
    Frame(layout);
    EXPECT_TRUE(Read("why"));
}

TEST_F(ImGuiUiRendererTest, AToggleSetsItsValue) {
    const ui::UiLayout layout = Layout(R"(<toggle bind="enabled" label="On"/>)");
    values.values["enabled"] = "0";
    Click(layout, kFirstElement);
    EXPECT_EQ(values.values["enabled"], "1");
}

TEST_F(ImGuiUiRendererTest, AButtonDoesItsAction) {
    const ui::UiLayout layout =
        Layout(R"(<button action="gazeora.gamepad.retry">Try again</button>)");
    Click(layout, kFirstElement);
    EXPECT_EQ(values.actions, std::vector<std::string>{"gazeora.gamepad.retry"});
}

TEST_F(ImGuiUiRendererTest, ADisabledButtonDoesNothing) {
    const ui::UiLayout layout = Layout(R"(<button action="go" enabled-if="ready">Go</button>)");
    Click(layout, kFirstElement);
    EXPECT_TRUE(values.actions.empty());
}

TEST(UiFieldDrawerTest, CarriesNumbersWithoutTheLocale) {
    EXPECT_EQ(UiFieldDrawer::FormatNumber(0.5f), "0.5");
    EXPECT_EQ(UiFieldDrawer::FormatNumber(180.0f), "180");
    EXPECT_EQ(UiFieldDrawer::ParseNumber("2.25", 0.0f), 2.25f);
    EXPECT_EQ(UiFieldDrawer::ParseNumber("lots", 7.0f), 7.0f);
}

}  // namespace
}  // namespace gazeora::app
