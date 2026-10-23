#pragma once

#include <imgui.h>

#include <array>
#include <optional>

#include "gazeora/input/models/tracking_snapshot.h"

namespace gazeora::app {

// The tracked hand drawn live: its bones in a box shaped like the camera's picture, mirrored
// like a selfie view, with the left hand too when it's in view. The fingers a gesture uses
// show in its color (ColorOf): the right index finger, with the left index while holding and
// folding to click and with the left pinch while scrolling; every finger, green, while the
// right hand is open.
class HandPreview {
public:
    using Joints = std::array<input::Vec2, input::kHandJointCount>;

    explicit HandPreview(float scale) : m_scale(scale) {}

    // Each gesture's color, shared with the status line.
    static ImU32 ColorOf(input::GestureMode mode);
    // Takes the full width available, up to a height limit.
    void Draw(const input::TrackingSnapshot& snapshot) const;

private:
    static constexpr int kJointsPerFinger = 4;

    // The color of each finger, if it's highlighted.
    struct Highlight {
        std::optional<ImU32> thumb;
        std::optional<ImU32> index;
        std::optional<ImU32> middle;
        std::optional<ImU32> others;  // ring and little fingers
    };
    // Where the hand is drawn.
    struct Canvas {
        ImDrawList* draw;
        ImVec2 origin;
        ImVec2 size;
        const Joints& joints;
    };

    static Highlight HighlightOf(const input::TrackingSnapshot& snapshot);
    // The left hand's: its index up while holding, its pinch while scrolling.
    static Highlight OtherHighlightOf(const input::TrackingSnapshot& snapshot);
    void DrawHand(const Canvas& canvas, const Highlight& highlight) const;
    void DrawFinger(const Canvas& canvas, const input::HandJoint (&joints)[kJointsPerFinger],
                    std::optional<ImU32> color) const;
    void DrawBone(const Canvas& canvas, input::HandJoint from, input::HandJoint to,
                  std::optional<ImU32> color) const;
    float S(float px) const { return px * m_scale; }

    float m_scale;
};

}  // namespace gazeora::app
