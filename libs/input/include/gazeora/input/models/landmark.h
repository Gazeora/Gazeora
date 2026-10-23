#pragma once

#include <cmath>

#include "gazeora/input/models/vec2.h"
#include "gazeora/input/models/vec3.h"

namespace gazeora::input {

// One hand joint in camera pixels; z is depth relative to the wrist, in the same units.
struct Landmark {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec2 Xy() const { return {x, y}; }
    Vec3 Xyz() const { return {x, y, z}; }
    float DistanceTo(const Landmark& other) const {
        float dx = x - other.x, dy = y - other.y, dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

}  // namespace gazeora::input
