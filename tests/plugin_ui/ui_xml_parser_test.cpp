// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_ui/parsing/ui_xml_parser.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "gazeora/plugin_ui/exceptions/ui_layout_error.h"
#include "gazeora/plugin_ui/models/ui_button.h"
#include "gazeora/plugin_ui/models/ui_choice.h"
#include "gazeora/plugin_ui/models/ui_group.h"
#include "gazeora/plugin_ui/models/ui_input.h"
#include "gazeora/plugin_ui/models/ui_separator.h"
#include "gazeora/plugin_ui/models/ui_slider.h"
#include "gazeora/plugin_ui/models/ui_table.h"
#include "gazeora/plugin_ui/models/ui_text.h"
#include "gazeora/plugin_ui/models/ui_toggle.h"
#include "support/fake_ui_values.h"

namespace gazeora::ui {
namespace {

template <typename Element>
const Element& As(const UiElements& elements, size_t index) {
    const auto* element = dynamic_cast<const Element*>(elements.at(index).get());
    if (!element) {
        throw std::runtime_error("element " + std::to_string(index) + " is another type");
    }
    return *element;
}

UiLayout Panel(const std::string& inside, const std::string& attributes = "") {
    return UiXmlParser::Parse(R"(<panel page="settings" title="Example")" + attributes + ">" +
                              inside + "</panel>");
}

// What parsing `xml` throws.
std::string ErrorOf(const std::string& xml) {
    try {
        UiXmlParser::Parse(xml);
    } catch (const UiLayoutError& error) {
        return error.what();
    }
    return "nothing";
}

std::string ErrorInPanel(const std::string& inside) {
    return ErrorOf(R"(<panel page="settings" title="Example">)" + inside + "</panel>");
}

TEST(UiXmlParserTest, ReadsThePanel) {
    const UiLayout layout = UiXmlParser::Parse(
        R"(<?xml version="1.0"?><panel page="controls" position="start" title="Game controller"
                  icon="game"/>)");
    EXPECT_EQ(layout.page, UiPage::Controls);
    EXPECT_EQ(layout.position, UiPosition::Start);
    EXPECT_EQ(layout.title, "Game controller");
    EXPECT_EQ(layout.icon, UiIcon::Game);
    EXPECT_TRUE(layout.children.empty());
    EXPECT_EQ(Panel("").position, UiPosition::End);
}

TEST(UiXmlParserTest, ReadsTextTogglesAndButtons) {
    const UiLayout layout = Panel(R"(
        <text tone="dim">  Two
            lines  </text>
        <text bind="status" tone-bind="status-tone"/>
        <toggle bind="enabled" label="On" help="Some
                help"/>
        <button action="retry" icon="refresh" visible-if="!ok">Try again</button>)");
    ASSERT_EQ(layout.children.size(), 4u);
    EXPECT_EQ(As<UiText>(layout.children, 0).text, "Two lines");
    EXPECT_EQ(As<UiText>(layout.children, 0).tone, UiTone::Dim);
    EXPECT_EQ(As<UiText>(layout.children, 1).bind, "status");
    EXPECT_EQ(As<UiText>(layout.children, 1).toneBind, "status-tone");
    EXPECT_EQ(As<UiToggle>(layout.children, 2).bind, "enabled");
    EXPECT_EQ(As<UiToggle>(layout.children, 2).help, "Some help");
    const UiButton& button = As<UiButton>(layout.children, 3);
    EXPECT_EQ(button.action, "retry");
    EXPECT_EQ(button.icon, UiIcon::Refresh);
    EXPECT_EQ(button.label, "Try again");
    testing::FakeUiValues values;
    EXPECT_TRUE(button.visibleIf.Holds(values));
    values.values["ok"] = "1";
    EXPECT_FALSE(button.visibleIf.Holds(values));
}

TEST(UiXmlParserTest, ReadsFields) {
    const UiLayout layout = Panel(R"(
        <slider bind="speed" label="Speed" min="30" max="720" format="%.0f deg/s"/>
        <slider bind="zone"/>
        <input bind="name" label="Name" hint="Your name"/>
        <choice bind="mode" label="Mode">
          <option value="fast">Fast</option>
          <option value="slow"/>
        </choice>)");
    const UiSlider& speed = As<UiSlider>(layout.children, 0);
    EXPECT_EQ(speed.min, 30.0f);
    EXPECT_EQ(speed.max, 720.0f);
    EXPECT_EQ(speed.format, "%.0f deg/s");
    EXPECT_EQ(As<UiSlider>(layout.children, 1).format, "%.2f");
    EXPECT_EQ(As<UiInput>(layout.children, 2).hint, "Your name");
    const UiChoice& mode = As<UiChoice>(layout.children, 3);
    ASSERT_EQ(mode.options.size(), 2u);
    EXPECT_EQ(mode.options[0].label, "Fast");
    EXPECT_EQ(mode.options[1].label, "slow");  // its value, without text
}

TEST(UiXmlParserTest, ReadsGroupsTablesAndSeparators) {
    const UiLayout layout = Panel(R"(
        <group layout="row" enabled-if="enabled">
          <button action="a">A</button>
          <button action="b">B</button>
        </group>
        <separator/>
        <table>
          <row><cell tone="dim">Move</cell><cell>Stick</cell></row>
          <row><cell>Only one</cell></row>
        </table>)");
    const UiGroup& group = As<UiGroup>(layout.children, 0);
    EXPECT_TRUE(group.row);
    EXPECT_EQ(group.children.size(), 2u);
    testing::FakeUiValues values;
    EXPECT_FALSE(group.enabledIf.Holds(values));
    As<UiSeparator>(layout.children, 1);
    const UiTable& table = As<UiTable>(layout.children, 2);
    EXPECT_EQ(table.columns, 2);
    ASSERT_EQ(table.rows.size(), 2u);
    EXPECT_EQ(table.rows[0][0].text, "Move");
    EXPECT_EQ(table.rows[0][0].tone, UiTone::Dim);
}

TEST(UiXmlParserTest, SaysWhatIsWrongAndWhere) {
    EXPECT_EQ(ErrorInPanel("\n<toggle bind=\"on\" lable=\"Typo\"/>"),
              "line 2: <toggle> has no attribute lable");
    EXPECT_EQ(ErrorInPanel("<slider/>"), "line 1: <slider> needs bind");
    EXPECT_EQ(ErrorInPanel("<checkbox bind=\"x\"/>"),
              "line 1: <checkbox> isn't an element a panel can have");
    EXPECT_EQ(ErrorInPanel("<text tone=\"pink\">Hi</text>"),
              "line 1: <text> unknown tone \"pink\"");
    EXPECT_EQ(ErrorInPanel("<button action=\"x\" icon=\"rocket\">Go</button>"),
              "line 1: <button> unknown icon \"rocket\"");
    EXPECT_EQ(ErrorInPanel("<slider bind=\"x\" min=\"5\" max=\"1\"/>"),
              "line 1: <slider> needs min below max");
    EXPECT_EQ(ErrorInPanel("<slider bind=\"x\" max=\"lots\"/>"),
              "line 1: <slider> max isn't a number: \"lots\"");
    EXPECT_EQ(ErrorInPanel("<choice bind=\"x\"/>"), "line 1: <choice> needs an <option>");
    EXPECT_EQ(ErrorInPanel("<text/>"), "line 1: <text> needs text inside it, or bind");
    EXPECT_EQ(ErrorInPanel("<toggle bind=\"x\"><text>No</text></toggle>"),
              "line 1: <toggle> can't have elements inside it");
    EXPECT_EQ(ErrorInPanel("<table><cell>No row</cell></table>"),
              "line 1: <cell> can't be in a <table>: use <row>");
    EXPECT_EQ(ErrorInPanel("<group layout=\"grid\"/>"),
              "line 1: <group> layout can only be \"row\"");
}

TEST(UiXmlParserTest, RefusesAPanelThatIsntOne) {
    EXPECT_EQ(ErrorOf(R"(<card page="home" title="X"/>)"),
              "line 1: <card> can't be the layout's root: use <panel>");
    EXPECT_EQ(ErrorOf(R"(<panel page="details" title="X"/>)"),
              "line 1: <panel> can't go on page \"details\"");
    EXPECT_EQ(ErrorOf(R"(<panel page="home"/>)"), "line 1: <panel> needs title");
    EXPECT_EQ(ErrorOf(""), "not valid XML: No document element found at character 0");
    EXPECT_EQ(ErrorOf("<panel page=\"home\" title=\"X\">").rfind("not valid XML: ", 0), 0u);
}

}  // namespace
}  // namespace gazeora::ui
