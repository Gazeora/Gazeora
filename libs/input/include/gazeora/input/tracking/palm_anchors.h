#pragma once

#include <vector>

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// The fixed anchor points of MediaPipe's 192x192 palm detector, from 0 to 1 across its
// input: each of its predictions is an offset from one of them.
class PalmAnchors {
public:
    PalmAnchors();

    const std::vector<Vec2>& All() const { return m_anchors; }

private:
    void AddGrid(int cellsPerSide, int anchorsPerCell);

    std::vector<Vec2> m_anchors;
};

}  // namespace gazeora::input
