// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "gazeora/plugin_ui/interfaces/ui_element_visitor.h"
#include "gazeora/plugin_ui/models/ui_condition.h"

namespace gazeora::ui {

// Composite pattern: one element of a panel's layout. Every element can be shown only while
// visibleIf holds, and used only while enabledIf does.
struct UiElement {
    virtual ~UiElement() = default;
    virtual void Accept(IUiElementVisitor& visitor) const = 0;

    UiCondition visibleIf;
    UiCondition enabledIf;
};

using UiElements = std::vector<std::shared_ptr<const UiElement>>;

// What every element type shares: it hands itself to the visitor.
template <typename Element>
struct UiVisitable : UiElement {
    void Accept(IUiElementVisitor& visitor) const override {
        visitor.Visit(static_cast<const Element&>(*this));
    }
};

}  // namespace gazeora::ui
