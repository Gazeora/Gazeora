// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/ui_input_buffers.h"

namespace gazeora::app {

std::string& UiInputBuffers::Begin(ImGuiID id, const std::string& value) {
    std::string& text = m_text[id];
    if (m_editing != id) {
        text = value;
    }
    return text;
}

void UiInputBuffers::End(ImGuiID id, bool active) {
    if (active) {
        m_editing = id;
    } else if (m_editing == id) {
        m_editing = 0;
    }
}

}  // namespace gazeora::app
