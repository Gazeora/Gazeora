#pragma once

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// A square, rotated region of the camera image that a model looks at. Crop coordinates run
// from -0.5 to 0.5 across the square; up in the crop points along `up` in the image.
class HandRoi {
public:
    HandRoi(Vec2 center, float size, Vec2 up) : m_center(center), m_size(size), m_up(up) {}

    // The image pixel under a crop point.
    Vec2 ToImage(Vec2 crop) const {
        return m_center + (m_up.Perpendicular() * crop.x - m_up * crop.y) * m_size;
    }
    Vec2 Center() const { return m_center; }
    float Size() const { return m_size; }
    Vec2 Up() const { return m_up; }

private:
    Vec2 m_center;
    float m_size;
    Vec2 m_up;  // unit length
};

}  // namespace gazeora::input
