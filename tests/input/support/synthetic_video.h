#pragma once

#include "gazeora/input/models/camera_frame.h"
#include "gazeora/input/models/vec2.h"

namespace gazeora::input::testing {

// Camera pictures made from a still photo of a hand, standing in for video of a moving one:
// the photo as a card in front of a 60-degree camera, moved, turned and tipped.
class SyntheticVideo {
public:
    static constexpr int kWidth = 640;
    static constexpr int kHeight = 480;

    explicit SyntheticVideo(CameraFrame photo) : m_photo(std::move(photo)) {}

    // The photo centred at `center` on a grey background, turned `rollDegrees` clockwise in
    // the picture, and its top tipped `pitchDegrees` toward the camera.
    CameraFrame Frame(Vec2 center, float rollDegrees = 0.0f, float pitchDegrees = 0.0f) const;
    // Two pictures side by side, the second mirrored: a photo of a right hand turns into a
    // left one, on the camera's right, where a user's left hand is.
    static CameraFrame SideBySide(const CameraFrame& right, const CameraFrame& left);

private:
    // Where in the photo an offset from the card's centre in the picture comes from.
    Vec2 PhotoPoint(Vec2 offset, float rollDegrees, float pitchDegrees) const;

    CameraFrame m_photo;
};

}  // namespace gazeora::input::testing
