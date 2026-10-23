#include "gazeora/host/stream/pacing_plan.h"

#include <gtest/gtest.h>

namespace gazeora {
namespace {

constexpr size_t kPacket = 1400;

TEST(PacingPlanTest, SendsAtFourTimesTheBitrate) {
    // 78 packets, 109 KB: at 80 Mbit/s that's about 11 ms.
    PacingPlan plan = PacingPlan::For(78, 78 * kPacket, 20000000, 30);
    EXPECT_EQ(plan.PacketsPerBurst() * plan.Bursts() >= 78, true);
    EXPECT_GE(plan.SpreadUs(), 8000);
    EXPECT_LE(plan.SpreadUs(), 11000);
}

TEST(PacingPlanTest, FinishesWithinHalfAFrameInterval) {
    // A key frame that would take 40 ms at four times the bitrate.
    for (uint32_t fps : {30u, 60u}) {
        PacingPlan plan = PacingPlan::For(286, 286 * kPacket, 20000000, fps);
        EXPECT_LE(plan.SpreadUs(), 1000000 / fps / 2) << fps << " fps";
        EXPECT_GE(plan.PacketsPerBurst() * plan.Bursts(), 286u);
    }
}

TEST(PacingPlanTest, KeepsBurstsAtLeastFourPackets) {
    PacingPlan plan = PacingPlan::For(10, 10 * kPacket, 3000000, 60);
    EXPECT_GE(plan.PacketsPerBurst(), PacingPlan::kMinPacketsPerBurst);
    EXPECT_EQ(plan.Bursts(), 3u);
}

TEST(PacingPlanTest, SendsASmallFrameInOneBurst) {
    PacingPlan plan = PacingPlan::For(3, 3 * kPacket, 20000000, 60);
    EXPECT_EQ(plan.Bursts(), 1u);
    EXPECT_EQ(plan.SpreadUs(), 0);
    EXPECT_EQ(PacingPlan::AtOnce(1).Bursts(), 1u);
    EXPECT_EQ(PacingPlan::For(0, 0, 20000000, 60).Bursts(), 0u);
}

}  // namespace
}  // namespace gazeora
