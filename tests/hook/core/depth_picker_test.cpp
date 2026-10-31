// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/depth_picker.h"

#include <gtest/gtest.h>

namespace gazeora::hook {
namespace {

int g_ids[4];  // addresses to stand for depth buffers
const void* const kScene = &g_ids[0];
const void* const kShadow = &g_ids[1];
const void* const kOther = &g_ids[2];

DepthPicker::Candidate Buffer(UINT width, UINT height, bool usable = true) {
    DepthPicker::Candidate candidate;
    candidate.width = width;
    candidate.height = height;
    candidate.usable = usable;
    return candidate;
}

TEST(DepthPickerTest, PicksTheOneWithMostDrawsInTheBackBuffersShape) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1920, 1080));
    picker.See(kShadow, Buffer(4096, 4096));  // square: a shadow map, however busy
    picker.Count(kScene, 300);
    picker.Count(kShadow, 900);
    EXPECT_EQ(picker.Pick(1920, 1080), kScene);
}

TEST(DepthPickerTest, TakesScaledRenderingButNotTinyBuffers) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1280, 720));  // dynamic resolution at two thirds
    picker.Count(kScene, 10);
    EXPECT_EQ(picker.Pick(1920, 1080), kScene);
    DepthPicker fresh;
    fresh.See(kOther, Buffer(480, 270));  // a quarter: some effect's buffer
    fresh.Count(kOther, 10);
    EXPECT_EQ(fresh.Pick(1920, 1080), nullptr);
}

TEST(DepthPickerTest, SkipsUnusableAndUndrawnBuffers) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1920, 1080, false));  // multisampled, say
    picker.Count(kScene, 500);
    picker.See(kOther, Buffer(1920, 1080));
    EXPECT_EQ(picker.Pick(1920, 1080), nullptr);
}

TEST(DepthPickerTest, KeepsItsPickUnlessAnotherHasClearlyMoreDraws) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1920, 1080));
    picker.Count(kScene, 100);
    ASSERT_EQ(picker.Pick(1920, 1080), kScene);

    picker.See(kScene, Buffer(1920, 1080));
    picker.See(kOther, Buffer(1920, 1080));
    picker.Count(kScene, 100);
    picker.Count(kOther, 140);
    EXPECT_EQ(picker.Pick(1920, 1080), kScene) << "a little more isn't enough";

    // Clearly more draws, but only for a moment: a menu drawing with two buffers by turns.
    for (int frame = 0; frame < 3 * DepthPicker::kFramesToSwitch; ++frame) {
        picker.See(kScene, Buffer(1920, 1080));
        picker.See(kOther, Buffer(1920, 1080));
        picker.Count(kScene, 100);
        picker.Count(kOther, frame % 2 ? 200 : 50);
        EXPECT_EQ(picker.Pick(1920, 1080), kScene) << frame;
    }

    // Clearly more for long enough: it takes over.
    for (int frame = 1; frame <= DepthPicker::kFramesToSwitch; ++frame) {
        picker.See(kScene, Buffer(1920, 1080));
        picker.See(kOther, Buffer(1920, 1080));
        picker.Count(kScene, 100);
        picker.Count(kOther, 200);
        EXPECT_EQ(picker.Pick(1920, 1080), frame < DepthPicker::kFramesToSwitch ? kScene : kOther)
            << frame;
    }
}

TEST(DepthPickerTest, KeepsItsPickThroughFramesWithoutIt) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1920, 1080));
    picker.Count(kScene, 100);
    ASSERT_EQ(picker.Pick(1920, 1080), kScene);
    EXPECT_EQ(picker.Pick(1920, 1080), kScene) << "a frame that draws nothing (a loading pause)";
}

TEST(DepthPickerTest, EachFrameCountsAfresh) {
    DepthPicker picker;
    picker.See(kScene, Buffer(1920, 1080));
    picker.Count(kScene, 100);
    picker.Pick(1920, 1080);
    EXPECT_FALSE(picker.Seen(kScene));
    picker.Count(kOther, 100);  // not seen this frame: not counted
    picker.See(kShadow, Buffer(1920, 1080));
    for (int frame = 0; frame < DepthPicker::kFramesToSwitch; ++frame) {
        picker.Count(kOther, 1000);
        EXPECT_EQ(picker.Pick(1920, 1080), kScene);
    }
}

}  // namespace
}  // namespace gazeora::hook
