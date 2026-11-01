// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "parsing/ui_element_factory.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "gazeora/plugin_ui/models/ui_button.h"
#include "gazeora/plugin_ui/models/ui_choice.h"
#include "gazeora/plugin_ui/models/ui_group.h"
#include "gazeora/plugin_ui/models/ui_input.h"
#include "gazeora/plugin_ui/models/ui_separator.h"
#include "gazeora/plugin_ui/models/ui_slider.h"
#include "gazeora/plugin_ui/models/ui_table.h"
#include "gazeora/plugin_ui/models/ui_text.h"
#include "gazeora/plugin_ui/models/ui_toggle.h"

namespace gazeora::ui {

namespace {

constexpr char kOption[] = "option";
constexpr char kRow[] = "row";
constexpr char kCell[] = "cell";
constexpr char kDefaultFormat[] = "%.2f";

// Reads the attributes every labeled control has.
template <typename Control>
void ReadLabel(UiXmlNode& node, Control& control) {
    control.bind = node.Required("bind");
    control.label = node.Optional("label");
    control.help = node.Optional("help");
}

}  // namespace

std::shared_ptr<const UiElement> UiElementFactory::Create(UiXmlNode node) {
    std::shared_ptr<UiElement> element = BuilderFor(node)(node);
    ReadConditions(node, *element);
    node.Finish();
    return element;
}

UiElements UiElementFactory::CreateChildren(const UiXmlNode& node) {
    UiElements children;
    for (UiXmlNode child : node.Children()) {
        children.push_back(Create(std::move(child)));
    }
    return children;
}

UiElementFactory::Builder UiElementFactory::BuilderFor(const UiXmlNode& node) {
    static const std::array<std::pair<const char*, Builder>, 9> kBuilders = {{
        {"text", Text},
        {"toggle", Toggle},
        {"button", Button},
        {"slider", Slider},
        {"input", Input},
        {"choice", Choice},
        {"group", Group},
        {"table", Table},
        {"separator", Separator},
    }};
    for (const auto& [name, builder] : kBuilders) {
        if (node.Name() == name) {
            return builder;
        }
    }
    node.Fail("isn't an element a panel can have");
}

std::shared_ptr<UiElement> UiElementFactory::Text(UiXmlNode& node) {
    RefuseChildren(node);
    auto text = std::make_shared<UiText>();
    text->text = node.Content();
    text->bind = node.Optional("bind");
    text->tone = node.Tone("tone");
    text->toneBind = node.Optional("tone-bind");
    if (text->text.empty() && text->bind.empty()) {
        node.Fail("needs text inside it, or bind");
    }
    return text;
}

std::shared_ptr<UiElement> UiElementFactory::Toggle(UiXmlNode& node) {
    RefuseChildren(node);
    auto toggle = std::make_shared<UiToggle>();
    ReadLabel(node, *toggle);
    return toggle;
}

std::shared_ptr<UiElement> UiElementFactory::Button(UiXmlNode& node) {
    RefuseChildren(node);
    auto button = std::make_shared<UiButton>();
    button->action = node.Required("action");
    button->icon = node.Icon("icon");
    button->label = node.Content();
    if (button->label.empty() && button->icon == UiIcon::None) {
        node.Fail("needs text inside it, or an icon");
    }
    return button;
}

std::shared_ptr<UiElement> UiElementFactory::Slider(UiXmlNode& node) {
    RefuseChildren(node);
    auto slider = std::make_shared<UiSlider>();
    ReadLabel(node, *slider);
    slider->min = node.Number("min", slider->min);
    slider->max = node.Number("max", slider->max);
    slider->format = node.Optional("format");
    if (slider->format.empty()) {
        slider->format = kDefaultFormat;
    }
    if (!(slider->min < slider->max)) {
        node.Fail("needs min below max");
    }
    return slider;
}

std::shared_ptr<UiElement> UiElementFactory::Input(UiXmlNode& node) {
    RefuseChildren(node);
    auto input = std::make_shared<UiInput>();
    ReadLabel(node, *input);
    input->hint = node.Optional("hint");
    return input;
}

std::shared_ptr<UiElement> UiElementFactory::Choice(UiXmlNode& node) {
    auto choice = std::make_shared<UiChoice>();
    ReadLabel(node, *choice);
    for (UiXmlNode option : node.Children()) {
        if (option.Name() != kOption) {
            option.Fail("can't be in a <choice>: use <option>");
        }
        RefuseChildren(option);
        UiOption read{option.Required("value"), option.Content()};
        if (read.label.empty()) {
            read.label = read.value;
        }
        option.Finish();
        choice->options.push_back(std::move(read));
    }
    if (choice->options.empty()) {
        node.Fail("needs an <option>");
    }
    return choice;
}

std::shared_ptr<UiElement> UiElementFactory::Group(UiXmlNode& node) {
    auto group = std::make_shared<UiGroup>();
    group->row = node.Flag("layout", kRow);
    group->children = CreateChildren(node);
    return group;
}

std::shared_ptr<UiElement> UiElementFactory::Table(UiXmlNode& node) {
    auto table = std::make_shared<UiTable>();
    for (UiXmlNode row : node.Children()) {
        if (row.Name() != kRow) {
            row.Fail("can't be in a <table>: use <row>");
        }
        std::vector<UiCell> cells;
        for (UiXmlNode cell : row.Children()) {
            if (cell.Name() != kCell) {
                cell.Fail("can't be in a <row>: use <cell>");
            }
            RefuseChildren(cell);
            cells.push_back({cell.Content(), cell.Tone("tone")});
            cell.Finish();
        }
        row.Finish();
        table->columns = std::max(table->columns, int(cells.size()));
        table->rows.push_back(std::move(cells));
    }
    if (table->columns == 0) {
        node.Fail("needs a <row> with a <cell>");
    }
    return table;
}

std::shared_ptr<UiElement> UiElementFactory::Separator(UiXmlNode& node) {
    RefuseChildren(node);
    return std::make_shared<UiSeparator>();
}

void UiElementFactory::ReadConditions(UiXmlNode& node, UiElement& element) {
    element.visibleIf = UiCondition(node.Optional("visible-if"));
    element.enabledIf = UiCondition(node.Optional("enabled-if"));
}

void UiElementFactory::RefuseChildren(const UiXmlNode& node) {
    if (!node.Children().empty()) {
        node.Fail("can't have elements inside it");
    }
}

}  // namespace gazeora::ui
