// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "keyboard/keyboard_view.h"

#include <algorithm>

#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kMaxWidth = 780.0f;
constexpr float kMargin = 16.0f;
constexpr float kPadding = 16.0f;
constexpr float kGap = 6.0f;
constexpr float kKeyHeight = 46.0f;
constexpr float kTextHeight = 40.0f;
constexpr float kHintHeight = 28.0f;
constexpr float kRounding = 10.0f;
constexpr float kKeyRounding = 6.0f;
constexpr int kRowWidth = 10;  // ordinary keys in a row
constexpr int kRows = 5;
constexpr char kCaret[] = "|";
constexpr char kHints[] =
    "A / Cross  type     X / Square  delete     Y / Triangle  space     LB / L1  Shift     "
    "RB / R1  symbols     B / Circle  done";

}  // namespace

void KeyboardView::Draw(const OnScreenKeyboard& keyboard, const std::string& text,
                        const Fonts& fonts) const {
    if (!keyboard.IsOpen()) {
        return;
    }
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    Metrics metrics = Measure();
    draw->AddRectFilled(metrics.origin, metrics.origin + ImVec2(metrics.width, Height()),
                        WithAlpha(kCardBg, 0.98f), S(kRounding));
    draw->AddRect(metrics.origin, metrics.origin + ImVec2(metrics.width, Height()), kBorder,
                  S(kRounding));
    DrawTyped(draw, metrics, text, fonts.key);
    DrawKeys(draw, metrics, keyboard, fonts.key);
    DrawHints(draw, metrics, fonts.caption);
}

KeyboardView::Metrics KeyboardView::Measure() const {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float width = std::min(viewport->WorkSize.x - S(kMargin) * 2.0f, S(kMaxWidth));
    ImVec2 origin(viewport->WorkPos.x + (viewport->WorkSize.x - width) * 0.5f,
                  viewport->WorkPos.y + viewport->WorkSize.y - Height() - S(kMargin));
    float unit = (width - S(kPadding) * 2.0f - S(kGap) * (kRowWidth - 1)) / kRowWidth;
    return {origin, width, unit};
}

float KeyboardView::Height() const {
    return S(kPadding) * 2.0f + S(kTextHeight) + S(kGap) + (S(kKeyHeight) + S(kGap)) * kRows +
           S(kHintHeight);
}

void KeyboardView::DrawTyped(ImDrawList* draw, const Metrics& metrics, const std::string& text,
                             ImFont* font) const {
    ImVec2 pos = metrics.origin + ImVec2(S(kPadding), S(kPadding));
    ImVec2 size(metrics.width - S(kPadding) * 2.0f, S(kTextHeight));
    draw->AddRectFilled(pos, pos + size, kInset, S(kKeyRounding));
    float textY = pos.y + (size.y - font->FontSize) * 0.5f;
    // The end of the text, where typing goes, stays in view.
    std::string shown = text + kCaret;
    float maxWidth = size.x - S(kPadding) * 2.0f;
    float textWidth = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, shown.c_str()).x;
    float x = pos.x + S(kPadding) - std::max(0.0f, textWidth - maxWidth);
    draw->PushClipRect(pos, pos + size, true);
    draw->AddText(font, font->FontSize, ImVec2(x, textY), kText, shown.c_str());
    draw->PopClipRect();
}

void KeyboardView::DrawKeys(ImDrawList* draw, const Metrics& metrics,
                            const OnScreenKeyboard& keyboard, ImFont* font) const {
    const KeyboardLayout& layout = keyboard.Layout();
    float top = metrics.origin.y + S(kPadding) + S(kTextHeight) + S(kGap);
    for (int row = 0; row < layout.RowCount(); ++row) {
        float x = metrics.origin.x + S(kPadding);
        float y = top + row * (S(kKeyHeight) + S(kGap));
        for (int column = 0; column < layout.ColumnCount(row); ++column) {
            const IKeyboardKey& key = layout.Key(row, column);
            float width = key.Span() * metrics.unit + (key.Span() - 1.0f) * S(kGap);
            bool chosen = row == keyboard.Row() && column == keyboard.Column();
            DrawKey(draw, ImVec2(x, y), ImVec2(width, S(kKeyHeight)), key.Label(keyboard.Shifted()),
                    chosen, font);
            x += width + S(kGap);
        }
    }
}

void KeyboardView::DrawKey(ImDrawList* draw, ImVec2 pos, ImVec2 size, const std::string& label,
                           bool chosen, ImFont* font) const {
    draw->AddRectFilled(pos, pos + size, chosen ? kAccent : kSecondary, S(kKeyRounding));
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label.c_str());
    draw->AddText(font, font->FontSize, pos + (size - textSize) * 0.5f, chosen ? kOnAccent : kText,
                  label.c_str());
}

void KeyboardView::DrawHints(ImDrawList* draw, const Metrics& metrics, ImFont* font) const {
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, kHints);
    float y = metrics.origin.y + Height() - S(kPadding) - S(kHintHeight) +
              (S(kHintHeight) - textSize.y) * 0.5f;
    float x = metrics.origin.x + (metrics.width - textSize.x) * 0.5f;
    draw->AddText(font, font->FontSize, ImVec2(x, y), kTextDim, kHints);
}

}  // namespace gazeora::app
