#include "ui/hand_preview.h"

#include <algorithm>

#include "ui/theme.h"

namespace gazeora::app {

namespace {

using input::GestureMode;
using input::HandJoint;

struct Bone {
    HandJoint from;
    HandJoint to;
};
// The palm, always plain: the wrist out to the knuckles, and across them.
constexpr Bone kPalmBones[] = {
    {HandJoint::Wrist, HandJoint::ThumbCmc},    {HandJoint::Wrist, HandJoint::IndexMcp},
    {HandJoint::Wrist, HandJoint::PinkyMcp},    {HandJoint::IndexMcp, HandJoint::MiddleMcp},
    {HandJoint::MiddleMcp, HandJoint::RingMcp}, {HandJoint::RingMcp, HandJoint::PinkyMcp},
};
// Each finger knuckle to tip (the thumb from its base), drawn in the color of what it's doing.
constexpr HandJoint kThumb[] = {HandJoint::ThumbCmc, HandJoint::ThumbMcp, HandJoint::ThumbIp,
                                HandJoint::ThumbTip};
constexpr HandJoint kIndex[] = {HandJoint::IndexMcp, HandJoint::IndexPip, HandJoint::IndexDip,
                                HandJoint::IndexTip};
constexpr HandJoint kMiddle[] = {HandJoint::MiddleMcp, HandJoint::MiddlePip, HandJoint::MiddleDip,
                                 HandJoint::MiddleTip};
constexpr HandJoint kRing[] = {HandJoint::RingMcp, HandJoint::RingPip, HandJoint::RingDip,
                               HandJoint::RingTip};
constexpr HandJoint kPinky[] = {HandJoint::PinkyMcp, HandJoint::PinkyPip, HandJoint::PinkyDip,
                                HandJoint::PinkyTip};
constexpr float kMaxHeight = 240.0f;
constexpr float kRounding = 8.0f;
constexpr float kBoneWidth = 3.0f;
constexpr float kHighlightWidth = 5.0f;
constexpr float kTipRadius = 7.0f;
constexpr float kHalf = 0.5f;

ImVec2 At(const HandPreview::Joints& joints, HandJoint joint, ImVec2 origin, ImVec2 size) {
    input::Vec2 point = joints[int(joint)];
    return ImVec2(origin.x + point.x * size.x, origin.y + point.y * size.y);
}

}  // namespace

ImU32 HandPreview::ColorOf(GestureMode mode) {
    switch (mode) {
        case GestureMode::Steering:
            return kAccent;
        case GestureMode::Holding:
            return kWarning;
        case GestureMode::Folded:
            return kDanger;
        case GestureMode::Scrolling:
            return kAccentEnd;
        case GestureMode::Resting:
            return kTextDim;
    }
    return kTextDim;
}

HandPreview::Highlight HandPreview::HighlightOf(const input::TrackingSnapshot& snapshot) {
    if (snapshot.handOpen) {
        return {kSuccess, kSuccess, kSuccess, kSuccess};
    }
    ImU32 color = ColorOf(snapshot.mode);
    switch (snapshot.mode) {
        case GestureMode::Steering:
            return {std::nullopt, color, std::nullopt, std::nullopt};
        case GestureMode::Holding:
        case GestureMode::Folded:
        case GestureMode::Scrolling:
            return {std::nullopt, color, std::nullopt, std::nullopt};
        case GestureMode::Resting:
            break;
    }
    return {};
}

HandPreview::Highlight HandPreview::OtherHighlightOf(const input::TrackingSnapshot& snapshot) {
    ImU32 color = ColorOf(snapshot.mode);
    switch (snapshot.mode) {
        case GestureMode::Holding:
        case GestureMode::Folded:
            return {std::nullopt, color, std::nullopt, std::nullopt};  // its index up
        case GestureMode::Scrolling:
            return {color, color, std::nullopt, std::nullopt};  // its pinch
        case GestureMode::Steering:
        case GestureMode::Resting:
            break;
    }
    return {};
}

void HandPreview::Draw(const input::TrackingSnapshot& snapshot) const {
    float width = ImGui::GetContentRegionAvail().x;
    float height = std::min(width / std::max(snapshot.cameraAspect, 0.1f), S(kMaxHeight));
    ImVec2 size(height * snapshot.cameraAspect, height);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.x += (width - size.x) * kHalf;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), kInset, S(kRounding));
    if (snapshot.state == input::TrackingState::Tracking) {
        if (snapshot.otherHand) {
            DrawHand({draw, origin, size, *snapshot.otherHand}, OtherHighlightOf(snapshot));
        }
        DrawHand({draw, origin, size, snapshot.joints}, HighlightOf(snapshot));
    }
    ImGui::Dummy(ImVec2(width, height));
}

void HandPreview::DrawHand(const Canvas& canvas, const Highlight& highlight) const {
    for (const Bone& bone : kPalmBones) {
        DrawBone(canvas, bone.from, bone.to, std::nullopt);
    }
    DrawFinger(canvas, kRing, highlight.others);
    DrawFinger(canvas, kPinky, highlight.others);
    DrawFinger(canvas, kThumb, highlight.thumb);
    DrawFinger(canvas, kMiddle, highlight.middle);
    DrawFinger(canvas, kIndex, highlight.index);
}

void HandPreview::DrawFinger(const Canvas& canvas, const HandJoint (&joints)[kJointsPerFinger],
                             std::optional<ImU32> color) const {
    for (int i = 0; i + 1 < kJointsPerFinger; ++i) {
        DrawBone(canvas, joints[i], joints[i + 1], color);
    }
    if (color) {
        canvas.draw->AddCircleFilled(
            At(canvas.joints, joints[kJointsPerFinger - 1], canvas.origin, canvas.size),
            S(kTipRadius), *color);
    }
}

void HandPreview::DrawBone(const Canvas& canvas, HandJoint from, HandJoint to,
                           std::optional<ImU32> color) const {
    canvas.draw->AddLine(At(canvas.joints, from, canvas.origin, canvas.size),
                         At(canvas.joints, to, canvas.origin, canvas.size),
                         color.value_or(kTextDim), S(color ? kHighlightWidth : kBoneWidth));
}

}  // namespace gazeora::app
