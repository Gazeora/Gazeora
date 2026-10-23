#pragma once

#include <algorithm>
#include <array>

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// The middle of the last three values, axis by axis: drops a jump that lasts a single
// picture, at the cost of one picture's delay.
class MedianOfThree {
public:
    Vec2 Filter(Vec2 value) {
        if (m_count == 0) {
            m_values.fill(value);
        }
        m_values[m_count++ % m_values.size()] = value;
        return {Middle(&Vec2::x), Middle(&Vec2::y)};
    }
    void Reset() { m_count = 0; }

private:
    float Middle(float Vec2::* axis) const {
        float a = m_values[0].*axis, b = m_values[1].*axis, c = m_values[2].*axis;
        return std::max(std::min(a, b), std::min(std::max(a, b), c));
    }

    std::array<Vec2, 3> m_values{};
    size_t m_count = 0;
};

}  // namespace gazeora::input
