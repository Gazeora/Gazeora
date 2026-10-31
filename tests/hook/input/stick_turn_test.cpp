// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/stick_turn.h"

#include <gtest/gtest.h>

namespace gazeora::hook::input {
namespace {

constexpr double kRead = 0.01;  // the game reads its controller 100 times a second

StickResponse Response(double degreesPerSecond, double deadZone = 0.0) {
    StickResponse response;
    response.degreesPerSecond = degreesPerSecond;
    response.deadZone = deadZone;
    return response;
}

TEST(StickTurnTest, TheFirstReadOnlyStarts) {
    StickTurn turn;
    StickPush push = turn.Next(30.0, -5.0, 1.0, Response(100));
    EXPECT_EQ(push.x, 0.0);
    EXPECT_EQ(push.y, 0.0);
}

TEST(StickTurnTest, PushesEnoughToTurnByTheNextRead) {
    StickTurn turn;
    turn.Next(0.0, 0.0, 1.0, Response(100));
    // Half a degree left and up in 10 ms: half of 100°/s.
    StickPush push = turn.Next(0.5, 0.5, 1.0 + kRead, Response(100));
    EXPECT_NEAR(push.x, -0.5, 1e-9);
    EXPECT_NEAR(push.y, 0.5, 1e-9);
    push = turn.Next(0.5, 0.5, 1.0 + 2 * kRead, Response(100));
    EXPECT_EQ(push.x, 0.0);  // made up already
}

TEST(StickTurnTest, LeavesWhatAFullPushCantMakeForTheNextRead) {
    StickTurn turn;
    turn.Next(0.0, 0.0, 1.0, Response(100));
    StickPush push = turn.Next(-1.5, 0.0, 1.0 + kRead, Response(100));  // 1.5° right
    EXPECT_NEAR(push.x, 1.0, 1e-9);
    push = turn.Next(-1.5, 0.0, 1.0 + 2 * kRead, Response(100));
    EXPECT_NEAR(push.x, 0.5, 1e-9);
}

TEST(StickTurnTest, StartsPastTheDeadZone) {
    StickTurn turn;
    turn.Next(0.0, 0.0, 1.0, Response(100, 0.2));
    StickPush push = turn.Next(0.0, -0.5, 1.0 + kRead, Response(100, 0.2));
    EXPECT_NEAR(push.y, -(0.2 + 0.8 * 0.5), 1e-9);
}

TEST(StickTurnTest, DropsTurnsFromBeforeAPause) {
    StickTurn turn;
    turn.Next(0.0, 0.0, 1.0, Response(100));
    StickPush push = turn.Next(20.0, 0.0, 2.0, Response(100));  // a second without reads
    EXPECT_EQ(push.x, 0.0);
    push = turn.Next(20.0, 0.0, 2.0 + kRead, Response(100));
    EXPECT_EQ(push.x, 0.0);
}

TEST(StickTurnTest, StaysStillWithoutAResponse) {
    StickTurn turn;
    turn.Next(0.0, 0.0, 1.0, Response(0));
    StickPush push = turn.Next(5.0, 5.0, 1.0 + kRead, Response(0));
    EXPECT_EQ(push.x, 0.0);
    EXPECT_EQ(push.y, 0.0);
}

}  // namespace
}  // namespace gazeora::hook::input
