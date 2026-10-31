// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/frame_mailbox.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "support/d3d_test_util.h"

namespace gazeora {
namespace {

class FrameMailboxTest : public ::testing::Test {
protected:
    void PostLive(uint64_t clock, uint8_t value) {
        std::vector<uint8_t> bgra(4 * 2 * 2, value);
        mailbox.PostLive(2, 2, clock, bgra);
    }

    FrameMailbox mailbox;
    Picture live, idle;
};

TEST_F(FrameMailboxTest, RepeatsTheLivePictureAtTheNewTime) {
    PostLive(100, 7);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    mailbox.PostRepeat(200);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    EXPECT_EQ(live.clock, 200u);
    EXPECT_EQ(live.bgra, std::vector<uint8_t>(16, 7));
}

TEST_F(FrameMailboxTest, ANewFrameWinsOverARepeat) {
    PostLive(100, 7);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    mailbox.PostRepeat(150);
    PostLive(200, 9);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    EXPECT_EQ(live.clock, 200u);
    EXPECT_EQ(live.bgra, std::vector<uint8_t>(16, 9));
    EXPECT_FALSE(mailbox.Take(0, live, idle)) << "the repeat went with the new frame";
}

TEST_F(FrameMailboxTest, IgnoresARepeatWithNothingToRepeat) {
    mailbox.PostRepeat(100);
    EXPECT_FALSE(mailbox.Take(0, live, idle));
}

TEST_F(FrameMailboxTest, HandsOverTexturesAndClearsThemForPixels) {
    test::WarpDevice warp = test::CreateWarpDevice();
    ASSERT_TRUE(warp.device) << "WARP D3D11.4 device unavailable";
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = test::CreateTexture(
        warp.device.Get(), 2, 2, DXGI_FORMAT_B8G8R8A8_UNORM, std::vector<uint32_t>(4));
    mailbox.PostLiveTexture(64, 64, 100, texture);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    EXPECT_EQ(live.nv12.Get(), texture.Get());
    EXPECT_TRUE(live.Complete()) << "a texture needs no pixels";
    PostLive(200, 9);
    ASSERT_TRUE(mailbox.Take(0, live, idle));
    EXPECT_FALSE(live.nv12) << "a frame in memory replaces the texture";
    EXPECT_EQ(live.bgra, std::vector<uint8_t>(16, 9));
}

}  // namespace
}  // namespace gazeora
