#pragma once

#include <cmath>

namespace gazeora::input {

// A direction or point in 3D camera space: x right, y down, z away from the camera.
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(Vec3 other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(Vec3 other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    float Dot(Vec3 other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 Cross(Vec3 other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    float Length() const { return std::sqrt(Dot(*this)); }
    // Unit length, or unchanged if zero.
    Vec3 Normalized() const {
        float length = Length();
        return length > 0.0f ? *this * (1.0f / length) : *this;
    }
};

}  // namespace gazeora::input
