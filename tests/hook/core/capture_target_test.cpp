// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/capture_target.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "gazeora/hook/core/hook_util.h"
#include "hook/fake_host.h"

namespace gazeora::hook {
namespace {

using test::FakeHost;

// Long enough for any previous target to count as no longer presenting.
constexpr DWORD kTargetTimeoutMs = 600;

class CaptureTargetTest : public ::testing::Test {
protected:
    void SetUp() override {
        FakeHost::Get().Reset();
        FakeHost::Get().Shared()->captureEnabled = 1;
    }
};

TEST_F(CaptureTargetTest, InactiveWhenHostDisablesCapture) {
    FakeHost::Get().Shared()->captureEnabled = 0;
    CaptureScope scope;
    EXPECT_FALSE(scope.Active());
}

TEST_F(CaptureTargetTest, ActiveWhenHostEnablesCapture) {
    LONGLONG before = QpcNow();
    CaptureScope scope;
    EXPECT_TRUE(scope.Active());
    EXPECT_GE(scope.PresentQpc(), before);
    EXPECT_LE(scope.PresentQpc(), QpcNow());
}

TEST_F(CaptureTargetTest, NestedScopeIsInactive) {
    CaptureScope outer;
    ASSERT_TRUE(outer.Active());
    CaptureScope inner;
    EXPECT_FALSE(inner.Active()) << "only try-locks, never waits";
}

TEST_F(CaptureTargetTest, LockReleasedWhenScopeEnds) {
    {
        CaptureScope first;
    }
    CaptureScope second;
    EXPECT_TRUE(second.Active());
}

TEST_F(CaptureTargetTest, ScopeInactiveDuringTeardown) {
    TeardownLock teardown;
    CaptureScope scope;
    EXPECT_FALSE(scope.Active());
}

TEST_F(CaptureTargetTest, TeardownWaitsForRunningCapture) {
    std::atomic<bool> tornDown{false};
    std::thread teardownThread;
    {
        CaptureScope scope;
        ASSERT_TRUE(scope.Active());
        teardownThread = std::thread([&] {
            TeardownLock teardown;
            tornDown = true;
        });
        Sleep(50);
        EXPECT_FALSE(tornDown) << "teardown ran during a capture";
    }
    teardownThread.join();
    EXPECT_TRUE(tornDown);
}

TEST_F(CaptureTargetTest, SelectTargetKeepsFirstPresenterUntilItGoesQuiet) {
    int presenterA = 0, presenterB = 0;
    Sleep(kTargetTimeoutMs);  // whatever an earlier test selected is now stale
    {
        CaptureScope scope;
        EXPECT_TRUE(scope.SelectTarget(&presenterA));
    }
    {
        CaptureScope scope;
        EXPECT_FALSE(scope.SelectTarget(&presenterB)) << "A is still presenting";
        EXPECT_TRUE(scope.SelectTarget(&presenterA));
    }
    Sleep(kTargetTimeoutMs);
    {
        CaptureScope scope;
        EXPECT_TRUE(scope.SelectTarget(&presenterB)) << "A stopped presenting";
        EXPECT_FALSE(scope.SelectTarget(&presenterA));
    }
}

}  // namespace
}  // namespace gazeora::hook
