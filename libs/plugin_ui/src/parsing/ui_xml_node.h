// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <pugixml.hpp>

#include <set>
#include <string>
#include <vector>

#include "gazeora/plugin_ui/models/ui_icon.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::ui {

// Adapter pattern: one XML element of a layout, read strictly. It remembers which attributes
// were read, so Finish() can refuse the ones nothing reads, and its errors name its line.
class UiXmlNode {
public:
    // `xml` is the whole layout, to count lines in; it must outlive this.
    UiXmlNode(pugi::xml_node node, const std::string& xml) : m_node(node), m_xml(xml) {}

    std::string Name() const { return m_node.name(); }
    std::string Optional(const char* attribute);
    // Throws if it's missing or empty.
    std::string Required(const char* attribute);
    float Number(const char* attribute, float fallback);
    UiTone Tone(const char* attribute);
    UiIcon Icon(const char* attribute);
    bool Flag(const char* attribute, const char* on);
    // The text inside it, its spaces and line breaks run together, as HTML shows them.
    std::string Content() const;
    std::vector<UiXmlNode> Children() const;

    // Throws if an attribute was never read: it's misspelled or doesn't belong here.
    void Finish() const;
    [[noreturn]] void Fail(const std::string& problem) const;

private:
    int Line() const;

    pugi::xml_node m_node;
    const std::string& m_xml;
    std::set<std::string> m_read;
};

}  // namespace gazeora::ui
