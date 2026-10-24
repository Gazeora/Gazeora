// The launcher's own controls, drawn with ImGui primitives in the theme's colors.
#pragma once

#include <imgui.h>

#include <string>

namespace gazeora::app {

// Where a label's visible part ends ("Play##id" shows "Play").
const char* LabelEnd(const char* label);

// UV rectangle that fills a rect of `rectAspect` with a texture of `texAspect`, cropping
// the overflow (CSS object-fit: cover).
void CoverUv(float texAspect, float rectAspect, ImVec2& uv0, ImVec2& uv1);

// Draws `text` clipped to `maxWidth`, ending in "..." if it doesn't fit.
void TextEllipsis(ImDrawList* draw, ImFont* font, ImVec2 pos, ImU32 color, const std::string& text,
                  float maxWidth);

// An invisible button a game controller can reach too (ImGui leaves those out by default).
// Its hover visuals then show on the focused one, as IsItemHovered() reports it hovered.
bool NavButton(const char* id, ImVec2 size);

// Call after a text field: a controller's select button starts typing in it, as Enter does on
// a keyboard. ImGui's own starts typing only with the top button.
void TypeOnSelect();

bool FilledButton(const char* label, ImVec2 size, ImU32 bg, ImU32 bgHover, ImU32 fg, ImFont* font,
                  float rounding, bool enabled = true);
// Text-only button, e.g. "Back" or "View all".
bool LinkButton(const char* label, ImFont* font, ImU32 color, ImU32 hoverColor);
// Primary action: violet fading into cyan, brighter on hover. `gradient` is the
// horizontal TextureCache gradient.
bool AccentButton(const char* label, ImVec2 size, ImFont* font, float rounding,
                  ImTextureID gradient, bool enabled = true);
bool Toggle(const char* id, bool* value, float scale);

// Section container with a rounded background. Pair with EndCard().
void BeginCard(const char* id, float scale, float width = 0.0f);
void EndCard();
void CardTitle(ImFont* font, const char* text);
void FieldLabel(ImFont* font, const char* text);
// A faint info icon on the current line; hovering or focusing it shows `text`, wrapped at
// `wrapWidth`.
void HelpMarker(const char* text, float wrapWidth);

}  // namespace gazeora::app
