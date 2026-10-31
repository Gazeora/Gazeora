// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/shared_channel.h"

#include <gtest/gtest.h>

#include <string>

#include "gazeora/hook/core/log.h"
#include "hook/fake_host.h"

namespace gazeora::hook {
namespace {

using test::FakeHost;

class SharedChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        FakeHost::Get().Reset();
        host = FakeHost::Get().Shared();
    }

    SharedHeader* host = nullptr;
};

ExportedHandles FakeHandles() {
    ExportedHandles handles;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        handles.slots[i] = reinterpret_cast<HANDLE>(uintptr_t(0x100 + i * 4));
    }
    handles.frameFence = reinterpret_cast<HANDLE>(uintptr_t(0x200));
    handles.hostFence = reinterpret_cast<HANDLE>(uintptr_t(0x204));
    return handles;
}

TEST_F(SharedChannelTest, OpensTheHostsBlock) {
    ASSERT_NE(Shared(), nullptr);
    EXPECT_NE(Shared(), host) << "the hook maps its own view";
    EXPECT_EQ(Shared()->magic, kSharedMagic);
    host->width = 1234;
    EXPECT_EQ(Shared()->width, 1234u) << "both views see the same memory";
}

TEST_F(SharedChannelTest, RejectsWrongMagicOrVersion) {
    host->magic = 0;
    EXPECT_FALSE(OpenSharedChannel());
    host->magic = kSharedMagic;
    host->version = kSharedVersion - 1;
    EXPECT_FALSE(OpenSharedChannel());
    host->version = kSharedVersion;
    EXPECT_TRUE(OpenSharedChannel());
}

TEST_F(SharedChannelTest, CaptureEnabledFollowsHost) {
    EXPECT_FALSE(CaptureEnabled());
    host->captureEnabled = 1;
    EXPECT_TRUE(CaptureEnabled());
    host->captureEnabled = 0;
    EXPECT_FALSE(CaptureEnabled());
}

TEST_F(SharedChannelTest, SetHookStateWritesHeader) {
    SetHookState(kHookInstalled);
    EXPECT_EQ(host->hookState, kHookInstalled);
    SetHookState(kHookError);
    EXPECT_EQ(host->hookState, kHookError);
}

TEST_F(SharedChannelTest, LaunchedByHostFollowsHostFlags) {
    EXPECT_FALSE(LaunchedByHost());
    host->hostFlags = kHostLaunchedGame;
    EXPECT_TRUE(LaunchedByHost());
}

TEST_F(SharedChannelTest, PublishResourcesDescribesResources) {
    host->frameIndex = 41;
    LUID luid = {0x1234, 7};
    int owner = 0;
    PublishResources(FakeHandles(), 640, 480, DXGI_FORMAT_R10G10B10A2_UNORM, luid, kApiD3D9,
                     &owner);

    EXPECT_EQ(host->texSeq, 2) << "seqlock bumped twice and left even";
    EXPECT_EQ(host->width, 640u);
    EXPECT_EQ(host->height, 480u);
    EXPECT_EQ(host->format, uint32_t(DXGI_FORMAT_R10G10B10A2_UNORM));
    EXPECT_EQ(host->api, uint32_t(kApiD3D9));
    EXPECT_EQ(host->adapterLuid.LowPart, 0x1234u);
    EXPECT_EQ(host->adapterLuid.HighPart, 7);
    for (UINT i = 0; i < kFrameSlots; ++i) {
        EXPECT_EQ(host->slotHandles[i], 0x100u + i * 4);
    }
    EXPECT_EQ(host->frameFenceHandle, 0x200u);
    EXPECT_EQ(host->hostFenceHandle, 0x204u);
    EXPECT_EQ(host->firstFrame, 42) << "first frame after the current one";
    EXPECT_STREQ(host->status, "capturing 640x480 via D3D9, DXGI format 24");

    PublishResources(FakeHandles(), 640, 480, DXGI_FORMAT_R10G10B10A2_UNORM, luid, kApiD3D9,
                     &owner);
    EXPECT_EQ(host->texSeq, 4);
}

TEST_F(SharedChannelTest, IsPublisherTracksLastPublisher) {
    int first = 0, second = 0;
    PublishResources(FakeHandles(), 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {}, kApiD3D11, &first);
    EXPECT_TRUE(IsPublisher(&first));
    EXPECT_FALSE(IsPublisher(&second));
    PublishResources(FakeHandles(), 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {}, kApiOpenGL, &second);
    EXPECT_FALSE(IsPublisher(&first));
    EXPECT_TRUE(IsPublisher(&second));
}

TEST_F(SharedChannelTest, FrameNumbersFollowFrameIndex) {
    EXPECT_EQ(InitialFenceValue(), 0u);
    EXPECT_EQ(NextFrameNumber(), 1);
    host->frameIndex = 99;
    EXPECT_EQ(InitialFenceValue(), 99u);
    EXPECT_EQ(NextFrameNumber(), 100);
}

TEST_F(SharedChannelTest, ClaimSlotWhenHostIsIdle) {
    EXPECT_TRUE(ClaimSlot(5, 0));
    EXPECT_EQ(host->writingFrame, 5) << "the claim is announced to the host";
    EXPECT_EQ(host->skippedFrames, 0);
}

TEST_F(SharedChannelTest, ClaimSlotSkipsSlotHostIsReading) {
    host->hostReadingFrame = 2;  // same slot as 2 + kFrameSlots
    EXPECT_FALSE(ClaimSlot(2 + kFrameSlots, 1));
    EXPECT_EQ(host->writingFrame, LONG64(2 + kFrameSlots)) << "announced even when skipping";
    EXPECT_EQ(host->skippedFrames, 1);
}

TEST_F(SharedChannelTest, ClaimSlotOnceHostFinishedReading) {
    host->hostReadingFrame = 2;
    EXPECT_TRUE(ClaimSlot(2 + kFrameSlots, 2)) << "host fence reached the frame it read";
    EXPECT_EQ(host->skippedFrames, 0);
}

TEST_F(SharedChannelTest, ClaimSlotIgnoresOtherSlots) {
    host->hostReadingFrame = 2;
    EXPECT_TRUE(ClaimSlot(3, 0));
    EXPECT_TRUE(ClaimSlot(4, 0));
    EXPECT_EQ(host->skippedFrames, 0);
}

TEST_F(SharedChannelTest, CountSkippedFrame) {
    CountSkippedFrame();
    CountSkippedFrame();
    EXPECT_EQ(host->skippedFrames, 2);
}

TEST_F(SharedChannelTest, FramePublishedNotifiesHost) {
    FramePublished(7, 123456);
    EXPECT_EQ(host->frameIndex, 7);
    EXPECT_EQ(host->lastPresentQpc, 123456);
    EXPECT_EQ(host->hookState, kHookCapturing);
    EXPECT_EQ(WaitForSingleObject(FakeHost::Get().FrameEvent(), 0), WAIT_OBJECT_0);
}

TEST_F(SharedChannelTest, ExportedHandlesCloseResetsHandles) {
    ExportedHandles handles;
    for (HANDLE& h : handles.slots) {
        h = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }
    handles.frameFence = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // hostFence left null: Close must cope with missing handles.
    handles.Close();
    for (HANDLE h : handles.slots) {
        EXPECT_EQ(h, nullptr);
    }
    EXPECT_EQ(handles.frameFence, nullptr);
    EXPECT_EQ(handles.hostFence, nullptr);
    handles.Close();  // idempotent
}

TEST_F(SharedChannelTest, SetStatusWritesStatus) {
    SetStatus("frame %d of %s", 3, "test");
    EXPECT_STREQ(host->status, "frame 3 of test");
}

TEST_F(SharedChannelTest, SetStatusTruncatesLongMessages) {
    std::string longText(1000, 'x');
    SetStatus("%s", longText.c_str());
    EXPECT_EQ(std::string(host->status), std::string(sizeof(host->status) - 1, 'x'));
}

}  // namespace
}  // namespace gazeora::hook
