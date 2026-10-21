#include "ui/theme.h"

#include <cstdint>

namespace gazeora::app {

ImU32 WithAlpha(ImU32 color, float alpha) {
    return (color & ~IM_COL32_A_MASK) | (ImU32(alpha * 255.0f + 0.5f) << IM_COL32_A_SHIFT);
}

ImU32 Shade(const float rgb[3], float brightness, float alpha) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(rgb[0] * brightness, rgb[1] * brightness, rgb[2] * brightness, alpha));
}

// A stable color per name, for games whose exe has no icon.
void NameColor(const std::wstring& name, float rgb[3]) {
    uint32_t hash = 2166136261u;
    for (wchar_t c : name) {
        hash = (hash ^ uint32_t(c)) * 16777619u;
    }
    ImGui::ColorConvertHSVtoRGB(float(hash % 360) / 360.0f, 0.55f, 0.75f, rgb[0], rgb[1], rgb[2]);
}

}  // namespace gazeora::app
