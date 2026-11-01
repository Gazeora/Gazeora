// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <map>
#include <string>

namespace gazeora::app {

// The text of plugin panels' text boxes between frames. While one is typed into, it keeps what
// was typed; otherwise it shows the bound value afresh each frame.
class UiInputBuffers {
public:
    // The text to edit in the box with ImGui id `id`, whose bound value is `value`.
    std::string& Begin(ImGuiID id, const std::string& value);
    // After the box is drawn: whether it's being typed into.
    void End(ImGuiID id, bool active);

private:
    std::map<ImGuiID, std::string> m_text;
    ImGuiID m_editing = 0;
};

}  // namespace gazeora::app
