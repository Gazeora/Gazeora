#pragma once

#include <imgui.h>

#include <string>

#include "gazeora/input/models/tracking_snapshot.h"

namespace gazeora::app {

// The one line under the hand preview: what tracking is doing, in the gesture's color.
class TrackingStatusLine {
public:
    explicit TrackingStatusLine(const input::TrackingSnapshot& snapshot);

    const std::string& Text() const { return m_text; }
    ImU32 Color() const { return m_color; }

private:
    static const char* ModeText(input::GestureMode mode);
    static const char* GestureText(const input::TrackingSnapshot& snapshot);
    static ImU32 GestureColor(const input::TrackingSnapshot& snapshot);
    void Describe(const input::TrackingSnapshot& snapshot);

    std::string m_text;
    ImU32 m_color;
};

}  // namespace gazeora::app
