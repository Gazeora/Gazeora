#include "ui/widgets.h"

#include <cfloat>
#include <cstring>

#include "ui/theme.h"

namespace gazeora::app {

const char* LabelEnd(const char* label) {
    const char* hidden = strstr(label, "##");
    return hidden ? hidden : label + strlen(label);
}

// UV rectangle that fills a rect of `rectAspect` with a texture of `texAspect`, cropping
// the overflow (CSS object-fit: cover).
void CoverUv(float texAspect, float rectAspect, ImVec2& uv0, ImVec2& uv1) {
    if (texAspect > rectAspect) {
        float w = rectAspect / texAspect;
        uv0 = ImVec2((1.0f - w) * 0.5f, 0.0f);
        uv1 = ImVec2((1.0f + w) * 0.5f, 1.0f);
    } else {
        float h = texAspect / rectAspect;
        uv0 = ImVec2(0.0f, (1.0f - h) * 0.5f);
        uv1 = ImVec2(1.0f, (1.0f + h) * 0.5f);
    }
}

// Draws `text` clipped to `maxWidth`, ending in "..." if it doesn't fit.
void TextEllipsis(ImDrawList* draw, ImFont* font, ImVec2 pos, ImU32 color, const std::string& text,
                  float maxWidth) {
    float size = font->FontSize;
    if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= maxWidth) {
        draw->AddText(font, size, pos, color, text.c_str());
        return;
    }
    float dots = font->CalcTextSizeA(size, FLT_MAX, 0.0f, "...").x;
    const char* end = text.c_str() + text.size();
    while (end > text.c_str() &&
           font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str(), end).x + dots > maxWidth) {
        // Step back one UTF-8 code point.
        do {
            --end;
        } while (end > text.c_str() && (static_cast<unsigned char>(*end) & 0xC0) == 0x80);
    }
    std::string clipped(text.c_str(), end);
    clipped += "...";
    draw->AddText(font, size, pos, color, clipped.c_str());
}

bool FilledButton(const char* label, ImVec2 size, ImU32 bg, ImU32 bgHover, ImU32 fg, ImFont* font,
                  float rounding, bool enabled) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = NavButton(label, size) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, pos + size, enabled ? (hovered ? bgHover : bg) : kDisabled, rounding);
    const char* end = LabelEnd(label);
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    draw->AddText(font, font->FontSize, pos + (size - textSize) * 0.5f, enabled ? fg : kTextFaint,
                  label, end);
    return pressed;
}

bool NavButton(const char* id, ImVec2 size) {
    return ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
}

// Text-only button, e.g. "Back" or "View all".
bool LinkButton(const char* label, ImFont* font, ImU32 color, ImU32 hoverColor) {
    const char* end = LabelEnd(label);
    ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = NavButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::GetWindowDrawList()->AddText(font, font->FontSize, pos, hovered ? hoverColor : color,
                                        label, end);
    return pressed;
}

// Primary action: violet fading into cyan, brighter on hover. `gradient` is the
// horizontal TextureCache gradient.
bool AccentButton(const char* label, ImVec2 size, ImFont* font, float rounding,
                  ImTextureID gradient, bool enabled) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = NavButton(label, size) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (enabled) {
        ImU32 start = hovered ? kAccentHover : kAccent;
        // The gradient texture fades out over its width; start it a third of the way in
        // so the left side stays solid violet.
        draw->AddRectFilled(pos, pos + size, kAccentEnd, rounding);
        draw->AddRectFilled(pos, pos + ImVec2(size.x * 0.35f, size.y), start, rounding,
                            ImDrawFlags_RoundCornersLeft);
        draw->AddImage(gradient, pos + ImVec2(size.x * 0.35f, 0), pos + size, ImVec2(0, 0),
                       ImVec2(1, 1), start);
    } else {
        draw->AddRectFilled(pos, pos + size, kDisabled, rounding);
    }
    const char* end = LabelEnd(label);
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    draw->AddText(font, font->FontSize, pos + (size - textSize) * 0.5f,
                  enabled ? kOnAccent : kTextFaint, label, end);
    return pressed;
}

bool Toggle(const char* id, bool* value, float scale) {
    ImVec2 size(40.0f * scale, 22.0f * scale);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = NavButton(id, size);
    if (pressed) {
        *value = !*value;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float radius = size.y * 0.5f;
    draw->AddRectFilled(pos, pos + size, *value ? kAccent : kSecondaryHover, radius);
    float knobX = *value ? pos.x + size.x - radius : pos.x + radius;
    draw->AddCircleFilled(ImVec2(knobX, pos.y + radius), radius - 3.0f * scale,
                          *value ? kOnAccent : kTextDim);
    return pressed;
}

// Section container with a rounded background. Pair with EndCard(). Flattened into the page,
// so a controller moves in and out of it like any other control.
void BeginCard(const char* id, float scale, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f * scale, 16.0f * scale));
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding |
                          ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * scale, 6.0f * scale));
}

void EndCard() {
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void CardTitle(ImFont* font, const char* text) {
    ImGui::PushFont(font);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
    ImGui::Spacing();
}

void FieldLabel(ImFont* font, const char* text) {
    ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void HelpMarker(const char* text, float wrapWidth) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
    ImGui::TextUnformatted(ICON_INFO);
    ImGui::PopStyleColor();
    // A button over the icon, so a controller can land on it and show the text.
    ImGui::SetCursorScreenPos(ImGui::GetItemRectMin());
    ImGui::PushID(text);
    NavButton("##help", ImGui::GetItemRectSize());
    ImGui::PopID();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(wrapWidth);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

}  // namespace gazeora::app
