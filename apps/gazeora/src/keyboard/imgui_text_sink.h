// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "keyboard/text_sink.h"

namespace gazeora::app {

// Adapter pattern: the on-screen keyboard as a real keyboard to ImGui, typing into whichever
// text field is active. Every field then saves as it would from the keyboard.
class ImGuiTextSink final : public ITextSink {
public:
    void Type(char32_t character) override;
    void Erase() override;
    // Enter: single-line fields keep the text and stop typing.
    void Finish() override;

private:
    static void Tap(int key);
};

}  // namespace gazeora::app
