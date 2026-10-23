#pragma once

#include <cmath>

namespace gazeora::input {

// A point or direction in 2D; image coordinates have y pointing down.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float scale) const { return {x * scale, y * scale}; }
    float Dot(Vec2 other) const { return x * other.x + y * other.y; }
    float Length() const { return std::sqrt(x * x + y * y); }
    // A quarter turn clockwise on screen: "right" when this is "up".
    Vec2 Perpendicular() const { return {-y, x}; }
};

}  // namespace gazeora::input
