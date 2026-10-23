#include "gazeora/input/tracking/palm_anchors.h"

namespace gazeora::input {

namespace {

// The model's feature maps: stride 8 (24x24 cells, 2 anchors each), then its three stride-16
// layers merged (12x12 cells, 6 anchors each). 2016 anchors in all.
struct AnchorGrid {
    int cellsPerSide;
    int anchorsPerCell;
};
constexpr AnchorGrid kGrids[] = {{24, 2}, {12, 6}};
constexpr float kCellCenter = 0.5f;

}  // namespace

PalmAnchors::PalmAnchors() {
    for (const AnchorGrid& grid : kGrids) {
        AddGrid(grid.cellsPerSide, grid.anchorsPerCell);
    }
}

void PalmAnchors::AddGrid(int cellsPerSide, int anchorsPerCell) {
    float cell = 1.0f / cellsPerSide;
    for (int row = 0; row < cellsPerSide; ++row) {
        for (int column = 0; column < cellsPerSide; ++column) {
            Vec2 center{(column + kCellCenter) * cell, (row + kCellCenter) * cell};
            m_anchors.insert(m_anchors.end(), anchorsPerCell, center);
        }
    }
}

}  // namespace gazeora::input
