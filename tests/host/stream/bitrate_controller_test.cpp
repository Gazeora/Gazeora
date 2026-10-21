#include <gtest/gtest.h>

#include "gazeora/host/stream/bitrate_controller.h"

namespace gazeora {
namespace {

using BC = BitrateController;

TEST(BitrateControllerTest, BacksOffOneStepPerIntervalWhileLossIsReported) {
    BitrateController controller;
    uint64_t now = 100000;
    controller.Report(1, 0.05f, now);
    EXPECT_TRUE(controller.Update(now, 1));
    EXPECT_EQ(controller.Step(), 1);
    EXPECT_FLOAT_EQ(controller.WorstLoss(), 0.05f);

    EXPECT_FALSE(controller.Update(now + BC::kBackOffEveryMs - 1, 1)) << "too soon";
    EXPECT_TRUE(controller.Update(now + BC::kBackOffEveryMs, 1));
    EXPECT_EQ(controller.Step(), 2);
}

TEST(BitrateControllerTest, StopsAtTheBottomOfTheLadder) {
    BitrateController controller;
    uint64_t now = 100000;
    for (int i = 0; i < 10; ++i, now += BC::kBackOffEveryMs) {
        controller.Report(1, 0.5f, now);
        controller.Update(now, 1);
    }
    EXPECT_EQ(controller.Step(), BC::kSteps - 1);
}

TEST(BitrateControllerTest, RecoversOnceReportsHaveBeenClean) {
    BitrateController controller;
    uint64_t now = 100000;
    controller.Report(1, 0.05f, now);
    controller.Update(now, 1);
    ASSERT_EQ(controller.Step(), 1);

    controller.Report(1, 0.0f, now + 1000);
    EXPECT_FALSE(controller.Update(now + BC::kRecoverAfterMs - 1, 1));
    EXPECT_TRUE(controller.Update(now + BC::kRecoverAfterMs, 1));
    EXPECT_EQ(controller.Step(), 0);
}

TEST(BitrateControllerTest, ForgetsStaleReportsAndResetsWithoutViewers) {
    BitrateController controller;
    uint64_t now = 100000;
    controller.Report(1, 0.05f, now);
    controller.Update(now, 1);
    controller.Update(now + BC::kLossWindowMs + 1, 1);
    EXPECT_FLOAT_EQ(controller.WorstLoss(), 0.0f);

    controller.Report(1, 0.05f, now + 20000);
    controller.Update(now + 20000, 1);
    ASSERT_GT(controller.Step(), 0);
    controller.Update(now + 20001, 0);
    EXPECT_EQ(controller.Step(), 0) << "the next player starts at full quality";
}

TEST(BitrateControllerTest, ScalesTheLadder) {
    EXPECT_EQ(BC::BitrateAt(0, 100), 8000000u);
    EXPECT_EQ(BC::BitrateAt(0, 70), 5600000u);
    EXPECT_EQ(BC::BitrateAt(BC::kSteps - 1, 100), 3000000u);
}

}  // namespace
}  // namespace gazeora
