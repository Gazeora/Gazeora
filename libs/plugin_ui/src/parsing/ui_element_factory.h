// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/plugin_ui/models/ui_element.h"
#include "parsing/ui_xml_node.h"

namespace gazeora::ui {

// Factory pattern: the layout element for each XML element inside a <panel>, by its name.
class UiElementFactory final {
public:
    UiElementFactory() = delete;

    // Throws UiLayoutError for an element that isn't one, or is written wrong.
    static std::shared_ptr<const UiElement> Create(UiXmlNode node);
    // Every element inside `node`, in order.
    static UiElements CreateChildren(const UiXmlNode& node);

private:
    using Builder = std::shared_ptr<UiElement> (*)(UiXmlNode& node);

    static std::shared_ptr<UiElement> Text(UiXmlNode& node);
    static std::shared_ptr<UiElement> Toggle(UiXmlNode& node);
    static std::shared_ptr<UiElement> Button(UiXmlNode& node);
    static std::shared_ptr<UiElement> Slider(UiXmlNode& node);
    static std::shared_ptr<UiElement> Input(UiXmlNode& node);
    static std::shared_ptr<UiElement> Choice(UiXmlNode& node);
    static std::shared_ptr<UiElement> Group(UiXmlNode& node);
    static std::shared_ptr<UiElement> Table(UiXmlNode& node);
    static std::shared_ptr<UiElement> Separator(UiXmlNode& node);

    static Builder BuilderFor(const UiXmlNode& node);
    // visible-if and enabled-if, which every element has.
    static void ReadConditions(UiXmlNode& node, UiElement& element);
    static void RefuseChildren(const UiXmlNode& node);
};

}  // namespace gazeora::ui
