// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::ui {

struct UiButton;
struct UiChoice;
struct UiGroup;
struct UiInput;
struct UiSeparator;
struct UiSlider;
struct UiTable;
struct UiText;
struct UiToggle;

// Visitor pattern: something done with each kind of layout element, such as drawing it.
class IUiElementVisitor {
public:
    virtual ~IUiElementVisitor() = default;

    virtual void Visit(const UiText& text) = 0;
    virtual void Visit(const UiToggle& toggle) = 0;
    virtual void Visit(const UiButton& button) = 0;
    virtual void Visit(const UiSlider& slider) = 0;
    virtual void Visit(const UiInput& input) = 0;
    virtual void Visit(const UiChoice& choice) = 0;
    virtual void Visit(const UiGroup& group) = 0;
    virtual void Visit(const UiTable& table) = 0;
    virtual void Visit(const UiSeparator& separator) = 0;
};

}  // namespace gazeora::ui
