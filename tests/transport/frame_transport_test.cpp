// End-to-end tests of the frame transport (common/shared.h): the real hook core and the
// real CaptureClient in one process, which plays the game. The hook side renders into
// its D3D11FrameRing on WARP; the host opens the same adapter and reads frames back.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/shared_channel.h"
#include "gazeora/hook/d3d11/d3d11_frame_ring.h"
#include "gazeora/host/capture/capture_client.h"
#include "support/d3d_test_util.h"

namespace gazeora {
namespace {

constexpr UINT kWidth = 4;
constexpr UINT kHeight = 2;
constexpr DWORD kTimeoutMs = 5000;

class FrameTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::wstring error;
        ASSERT_TRUE(client.Create(GetCurrentProcessId(), error)) << error;
        ASSERT_TRUE(hook::OpenSharedChannel());
        game = test::CreateWarpDevice();
        ASSERT_TRUE(game.device && game.context) << "WARP D3D11.4 device unavailable";
        ASSERT_TRUE(
            ring.Create(game.device.Get(), kWidth, kHeight, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
        client.SetCaptureEnabled(true);
        // The hook's view from an earlier test keeps the block alive, so start from its counts.
        skippedBefore = client.Shared().skippedFrames;
    }

    // What a present hook does: claim the next slot, fill it, publish it.
    LONG64 PresentFrame(uint32_t color) {
        LONG64 frame = hook::NextFrameNumber();
        EXPECT_TRUE(ring.Claim(frame));
        std::vector<uint32_t> pixels(kWidth * kHeight, color);
        game.context->UpdateSubresource(ring.Slot(frame), 0, nullptr, pixels.data(), kWidth * 4, 0);
        ring.Publish(game.context.Get(), frame, hook::QpcNow(), true);
        return frame;
    }

    std::vector<uint32_t> HostPixels() {
        return test::ReadTexture(client.Device(), client.Frame());
    }

    CaptureClient client;  // created first: it owns the shared block
    test::WarpDevice game;
    hook::D3D11FrameRing ring;
    LONG64 skippedBefore = 0;
};

TEST_F(FrameTransportTest, HookSeesHostState) {
    EXPECT_TRUE(hook::CaptureEnabled());
    client.SetCaptureEnabled(false);
    EXPECT_FALSE(hook::CaptureEnabled());
    hook::SetHookState(kHookInstalled);
    EXPECT_EQ(client.Shared().hookState, kHookInstalled);
}

TEST_F(FrameTransportTest, HostReceivesFrame) {
    LONG64 frame = PresentFrame(0xFF336699);
    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);

    ASSERT_NE(client.Frame(), nullptr);
    D3D11_TEXTURE2D_DESC desc;
    client.Frame()->GetDesc(&desc);
    EXPECT_EQ(desc.Width, kWidth);
    EXPECT_EQ(desc.Height, kHeight);
    EXPECT_EQ(desc.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(HostPixels(), std::vector<uint32_t>(kWidth * kHeight, 0xFF336699));

    EXPECT_EQ(client.Shared().hostReadingFrame, frame);
    EXPECT_EQ(client.Shared().hookState, kHookCapturing);
    EXPECT_GE(client.LastHandoffMs(), 0.0);
    EXPECT_LT(client.LastHandoffMs(), double(kTimeoutMs));
}

TEST_F(FrameTransportTest, HostReceivesFramesAcrossTheWholeRing) {
    for (uint32_t i = 0; i < 3 * kFrameSlots; ++i) {
        uint32_t color = 0xFF000000 | (i * 0x111111);
        PresentFrame(color);
        ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame) << i;
        EXPECT_EQ(HostPixels(), std::vector<uint32_t>(kWidth * kHeight, color)) << i;
    }
    EXPECT_EQ(client.Shared().skippedFrames, skippedBefore);
}

TEST_F(FrameTransportTest, HostReadReleasesSlotForReuse) {
    LONG64 frame = PresentFrame(0xFF0000FF);
    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);
    // The host signals its fence on the GPU; once that lands the hook may reuse the slot.
    LONG64 reuse = frame + kFrameSlots;
    bool claimed = false;
    for (int i = 0; i < 100 && !claimed; ++i, Sleep(10)) {
        claimed = ring.Claim(reuse);
    }
    EXPECT_TRUE(claimed);
}

TEST_F(FrameTransportTest, HookSkipsSlotHostIsStillReading) {
    LONG64 frame = PresentFrame(0xFF0000FF);
    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);
    // Pretend the host fence never advanced: the slot is still being read.
    EXPECT_FALSE(hook::ClaimSlot(frame + kFrameSlots, UINT64(frame) - 1));
    EXPECT_EQ(client.Shared().skippedFrames, skippedBefore + 1);
}

TEST_F(FrameTransportTest, HostIgnoresAlreadyReadFrame) {
    PresentFrame(0xFF00FF00);
    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);
    // A spurious wake-up with no new frame.
    HANDLE event =
        OpenEventW(EVENT_MODIFY_STATE, FALSE, FrameEventName(GetCurrentProcessId()).c_str());
    ASSERT_NE(event, nullptr);
    SetEvent(event);
    CloseHandle(event);
    EXPECT_EQ(client.WaitForFrame(100), CaptureClient::WaitResult::Timeout);
}

TEST_F(FrameTransportTest, HostFollowsRecreatedResources) {
    PresentFrame(0xFF111111);
    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);

    // The game resized: the hook recreates and republishes its ring.
    ASSERT_TRUE(ring.Create(game.device.Get(), 8, 6, DXGI_FORMAT_R8G8B8A8_UNORM, kApiD3D11));
    LONG64 frame = hook::NextFrameNumber();
    ASSERT_TRUE(ring.Claim(frame));
    std::vector<uint32_t> pixels(8 * 6, 0xFF223344);
    game.context->UpdateSubresource(ring.Slot(frame), 0, nullptr, pixels.data(), 8 * 4, 0);
    ring.Publish(game.context.Get(), frame, hook::QpcNow(), true);

    ASSERT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Frame);
    D3D11_TEXTURE2D_DESC desc;
    client.Frame()->GetDesc(&desc);
    EXPECT_EQ(desc.Width, 8u);
    EXPECT_EQ(desc.Height, 6u);
    EXPECT_EQ(desc.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(HostPixels(), pixels);
}

TEST_F(FrameTransportTest, HostDoesNotReadSlotHookIsOverwriting) {
    PresentFrame(0xFF111111);
    // The hook has already lapped the ring and is writing over the published frame's slot.
    InterlockedExchange64(&hook::Shared()->writingFrame, hook::Shared()->frameIndex + kFrameSlots);
    EXPECT_EQ(client.WaitForFrame(kTimeoutMs), CaptureClient::WaitResult::Timeout);
    EXPECT_EQ(client.Shared().hostReadingFrame, 0) << "read announcement withdrawn";
}

}  // namespace
}  // namespace gazeora
