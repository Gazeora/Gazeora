#include "ui/ui_scale.h"

#include <imgui_impl_win32.h>

#include <algorithm>

namespace gazeora::app {

namespace {

// Window sizes the layout is designed for, at 100% scaling.
constexpr float kMinWidth = 980.0f;
constexpr float kMinHeight = 640.0f;
constexpr float kDefaultWidth = 1360.0f;
constexpr float kDefaultHeight = 860.0f;
// Windows' 100% text reads small for a launcher used from the couch or the headset.
constexpr float kComfortZoom = 1.25f;
constexpr float kNoZoom = 1.0f;

float Width(const RECT& rect) { return float(rect.right - rect.left); }
float Height(const RECT& rect) { return float(rect.bottom - rect.top); }

}  // namespace

UiScale UiScale::ForMonitor(HMONITOR monitor) {
    MONITORINFO info = {sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    // Grow only as far as the smallest window still fits the screen.
    float fit =
        std::min(Width(info.rcWork) / (kMinWidth * dpi), Height(info.rcWork) / (kMinHeight * dpi));
    float zoom = std::clamp(fit, kNoZoom, kComfortZoom);
    return UiScale(dpi * zoom, info.rcWork);
}

SIZE UiScale::MinWindowSize() const {
    return {LONG(kMinWidth * m_factor), LONG(kMinHeight * m_factor)};
}

SIZE UiScale::DefaultWindowSize() const {
    return {LONG(std::min(kDefaultWidth * m_factor, Width(m_workArea))),
            LONG(std::min(kDefaultHeight * m_factor, Height(m_workArea)))};
}

}  // namespace gazeora::app
