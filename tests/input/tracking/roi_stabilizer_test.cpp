#include "gazeora/input/tracking/roi_stabilizer.h"

#include <gtest/gtest.h>

namespace gazeora::input {
namespace {

const Vec2 kUp{0.0f, -1.0f};

TEST(RoiStabilizerTest, IgnoresSmallChanges) {
    RoiStabilizer stabilizer;
    stabilizer.Update(HandRoi({100.0f, 100.0f}, 200.0f, kUp));
    HandRoi kept = stabilizer.Update(HandRoi({105.0f, 98.0f}, 210.0f, {0.05f, -0.9987f}));
    EXPECT_FLOAT_EQ(kept.Center().x, 100.0f);
    EXPECT_FLOAT_EQ(kept.Size(), 200.0f);
}

TEST(RoiStabilizerTest, FollowsTheHandWhenItMoves) {
    RoiStabilizer stabilizer;
    stabilizer.Update(HandRoi({100.0f, 100.0f}, 200.0f, kUp));
    EXPECT_FLOAT_EQ(stabilizer.Update(HandRoi({115.0f, 100.0f}, 200.0f, kUp)).Center().x, 115.0f);
    EXPECT_FLOAT_EQ(stabilizer.Update(HandRoi({115.0f, 100.0f}, 240.0f, kUp)).Size(), 240.0f);
    HandRoi turned = stabilizer.Update(HandRoi({115.0f, 100.0f}, 240.0f, {0.2f, -0.98f}));
    EXPECT_FLOAT_EQ(turned.Up().x, 0.2f);
}

TEST(RoiStabilizerTest, ResetTakesTheNextRegion) {
    RoiStabilizer stabilizer;
    stabilizer.Update(HandRoi({100.0f, 100.0f}, 200.0f, kUp));
    stabilizer.Reset();
    EXPECT_FLOAT_EQ(stabilizer.Update(HandRoi({101.0f, 100.0f}, 200.0f, kUp)).Center().x, 101.0f);
}

}  // namespace
}  // namespace gazeora::input
