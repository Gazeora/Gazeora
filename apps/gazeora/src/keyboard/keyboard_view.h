// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <string>

#include "keyboard/on_screen_keyboard.h"

namespace gazeora::app {

// Draws the on-screen keyboard over everything at the bottom of the window, so the stream and
// the headset show it too: the text so far, the keys with the chosen one lit, and the
// controller's shortcuts.
class KeyboardView {
public:
    struct Fonts {
        ImFont* key;
        ImFont* caption;
    };

    explicit KeyboardView(float scale) : m_scale(scale) {}

    void Draw(const OnScreenKeyboard& keyboard, const std::string& text, const Fonts& fonts) const;

private:
    struct Metrics {
        ImVec2 origin;  // the panel's top left
        float width;
        float unit;  // an ordinary key's width
    };

    Metrics Measure() const;
    void DrawTyped(ImDrawList* draw, const Metrics& metrics, const std::string& text,
                   ImFont* font) const;
    void DrawKeys(ImDrawList* draw, const Metrics& metrics, const OnScreenKeyboard& keyboard,
                  ImFont* font) const;
    void DrawKey(ImDrawList* draw, ImVec2 pos, ImVec2 size, const std::string& label, bool chosen,
                 ImFont* font) const;
    void DrawHints(ImDrawList* draw, const Metrics& metrics, ImFont* font) const;
    float Height() const;
    float S(float px) const { return px * m_scale; }

    float m_scale;
};

}  // namespace gazeora::app
