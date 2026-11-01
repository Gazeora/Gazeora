// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "parsing/ui_xml_node.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <optional>

#include "gazeora/plugin_ui/exceptions/ui_layout_error.h"

namespace gazeora::ui {

namespace {

bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string Quoted(const std::string& text) { return "\"" + text + "\""; }

}  // namespace

std::string UiXmlNode::Optional(const char* attribute) {
    m_read.insert(attribute);
    return m_node.attribute(attribute).as_string();
}

std::string UiXmlNode::Required(const char* attribute) {
    std::string value = Optional(attribute);
    if (value.empty()) {
        Fail(std::string("needs ") + attribute);
    }
    return value;
}

float UiXmlNode::Number(const char* attribute, float fallback) {
    const std::string text = Optional(attribute);
    if (text.empty()) {
        return fallback;
    }
    char* end = nullptr;
    errno = 0;
    const float number = std::strtof(text.c_str(), &end);
    if (*end != '\0' || errno != 0 || !std::isfinite(number)) {
        Fail(std::string(attribute) + " isn't a number: " + Quoted(text));
    }
    return number;
}

UiTone UiXmlNode::Tone(const char* attribute) {
    const std::string id = Optional(attribute);
    if (id.empty()) {
        return UiTone::Normal;
    }
    const std::optional<UiTone> tone = UiTones::Parse(id);
    if (!tone) {
        Fail(std::string("unknown ") + attribute + " " + Quoted(id));
    }
    return *tone;
}

UiIcon UiXmlNode::Icon(const char* attribute) {
    const std::string name = Optional(attribute);
    const std::optional<UiIcon> icon = UiIcons::Parse(name);
    if (!icon) {
        Fail("unknown icon " + Quoted(name));
    }
    return *icon;
}

bool UiXmlNode::Flag(const char* attribute, const char* on) {
    const std::string value = Optional(attribute);
    if (!value.empty() && value != on) {
        Fail(std::string(attribute) + " can only be " + Quoted(on));
    }
    return value == on;
}

std::string UiXmlNode::Content() const {
    std::string collapsed;
    bool space = false;
    for (char c : std::string(m_node.text().get())) {
        if (IsSpace(c)) {
            space = !collapsed.empty();
            continue;
        }
        if (space) {
            collapsed += ' ';
            space = false;
        }
        collapsed += c;
    }
    return collapsed;
}

std::vector<UiXmlNode> UiXmlNode::Children() const {
    std::vector<UiXmlNode> children;
    for (pugi::xml_node child : m_node.children()) {
        if (child.type() == pugi::node_element) {
            children.emplace_back(child, m_xml);
        }
    }
    return children;
}

void UiXmlNode::Finish() const {
    for (pugi::xml_attribute attribute : m_node.attributes()) {
        if (m_read.count(attribute.name()) == 0) {
            Fail(std::string("has no attribute ") + attribute.name());
        }
    }
}

void UiXmlNode::Fail(const std::string& problem) const {
    throw UiLayoutError("line " + std::to_string(Line()) + ": <" + Name() + "> " + problem);
}

int UiXmlNode::Line() const {
    const ptrdiff_t offset = std::clamp<ptrdiff_t>(m_node.offset_debug(), 0, m_xml.size());
    return 1 + int(std::count(m_xml.begin(), m_xml.begin() + offset, '\n'));
}

}  // namespace gazeora::ui
