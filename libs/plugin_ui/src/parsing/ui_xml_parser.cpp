// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_ui/parsing/ui_xml_parser.h"

#include <pugixml.hpp>

#include <optional>

#include "gazeora/plugin_ui/exceptions/ui_layout_error.h"
#include "parsing/ui_element_factory.h"
#include "parsing/ui_xml_node.h"

namespace gazeora::ui {

namespace {

constexpr char kPanel[] = "panel";
constexpr char kStart[] = "start";

UiPage PageOf(UiXmlNode& panel) {
    const std::string id = panel.Required("page");
    const std::optional<UiPage> page = UiPages::Parse(id);
    if (!page) {
        panel.Fail("can't go on page \"" + id + "\"");
    }
    return *page;
}

UiLayout LayoutOf(UiXmlNode panel) {
    if (panel.Name() != kPanel) {
        panel.Fail("can't be the layout's root: use <panel>");
    }
    UiLayout layout;
    layout.page = PageOf(panel);
    layout.position = panel.Flag("position", kStart) ? UiPosition::Start : UiPosition::End;
    layout.title = panel.Required("title");
    layout.icon = panel.Icon("icon");
    layout.children = UiElementFactory::CreateChildren(panel);
    panel.Finish();
    return layout;
}

}  // namespace

UiLayout UiXmlParser::Parse(const std::string& xml) {
    pugi::xml_document document;
    // Attributes run their spaces and line breaks together, as text inside elements does.
    const pugi::xml_parse_result result = document.load_buffer(
        xml.data(), xml.size(), pugi::parse_default | pugi::parse_wnorm_attribute,
        pugi::encoding_utf8);
    if (!result) {
        throw UiLayoutError("not valid XML: " + std::string(result.description()) +
                            " at character " + std::to_string(result.offset));
    }
    pugi::xml_node root = document.document_element();
    if (!root) {
        throw UiLayoutError("no <panel>");
    }
    return LayoutOf(UiXmlNode(root, xml));
}

}  // namespace gazeora::ui
