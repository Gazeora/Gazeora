#include "gazeora/input/tracking/palm_anchors.h"

#include <gtest/gtest.h>

namespace gazeora::input {
namespace {

TEST(PalmAnchorsTest, MatchesTheModelsLayout) {
    PalmAnchors anchors;
    const std::vector<Vec2>& all = anchors.All();
    ASSERT_EQ(all.size(), 2016u);
    // Stride 8: two anchors per cell of a 24x24 grid, row by row.
    EXPECT_FLOAT_EQ(all[0].x, 0.5f / 24);
    EXPECT_FLOAT_EQ(all[1].x, 0.5f / 24);
    EXPECT_FLOAT_EQ(all[2].x, 1.5f / 24);
    EXPECT_FLOAT_EQ(all[48].y, 1.5f / 24);
    // Stride 16: six anchors per cell of a 12x12 grid.
    EXPECT_FLOAT_EQ(all[1152].x, 0.5f / 12);
    EXPECT_FLOAT_EQ(all[1158].x, 1.5f / 12);
    EXPECT_FLOAT_EQ(all.back().x, 11.5f / 12);
    EXPECT_FLOAT_EQ(all.back().y, 11.5f / 12);
}

}  // namespace
}  // namespace gazeora::input
