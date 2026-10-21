#include "gazeora/hook/d3d11/d3d11_frame_ring.h"

#include <gtest/gtest.h>

#include <string>

#include "hook/fake_host.h"
#include "support/d3d_test_util.h"

namespace gazeora::hook {
namespace {

using test::FakeHost;

class D3D11FrameRingTest : public ::testing::Test {
protected:
    void SetUp() override {
        FakeHost::Get().Reset();
        host = FakeHost::Get().Shared();
        warp = test::CreateWarpDevice();
        ASSERT_TRUE(warp.device && warp.context) << "WARP D3D11.4 device unavailable";
    }

    SharedHeader* host = nullptr;
    test::WarpDevice warp;
};

TEST_F(D3D11FrameRingTest, CreatePublishesSharedResources) {
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 320, 200, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));

    EXPECT_EQ(ring.Width(), 320u);
    EXPECT_EQ(ring.Height(), 200u);
    EXPECT_EQ(ring.Format(), DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(ring.Generation(), 1u);
    EXPECT_TRUE(ring.Matches(320, 200, DXGI_FORMAT_B8G8R8A8_UNORM));

    EXPECT_EQ(host->texSeq, 2);
    EXPECT_EQ(host->width, 320u);
    EXPECT_EQ(host->height, 200u);
    EXPECT_EQ(host->format, uint32_t(DXGI_FORMAT_B8G8R8A8_UNORM));
    EXPECT_EQ(host->api, uint32_t(kApiD3D11));
    for (UINT i = 0; i < kFrameSlots; ++i) {
        ASSERT_NE(ring.SlotHandle(i), nullptr);
        EXPECT_EQ(host->slotHandles[i], reinterpret_cast<uint64_t>(ring.SlotHandle(i)));
        D3D11_TEXTURE2D_DESC desc;
        ring.Slot(i)->GetDesc(&desc);
        EXPECT_EQ(desc.Width, 320u);
        EXPECT_TRUE(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
    }
    EXPECT_EQ(host->frameFenceHandle, reinterpret_cast<uint64_t>(ring.FrameFenceHandle()));
    EXPECT_NE(host->hostFenceHandle, 0u);

    // The published LUID must be the device's own adapter.
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc;
    ASSERT_HRESULT_SUCCEEDED(warp.device.As(&dxgiDevice));
    ASSERT_HRESULT_SUCCEEDED(dxgiDevice->GetAdapter(&adapter));
    ASSERT_HRESULT_SUCCEEDED(adapter->GetDesc(&adapterDesc));
    EXPECT_EQ(host->adapterLuid.LowPart, adapterDesc.AdapterLuid.LowPart);
    EXPECT_EQ(host->adapterLuid.HighPart, adapterDesc.AdapterLuid.HighPart);
}

TEST_F(D3D11FrameRingTest, SlotsWrapAroundTheRing) {
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
    EXPECT_EQ(ring.Slot(1), ring.Slot(1 + kFrameSlots));
    EXPECT_NE(ring.Slot(1), ring.Slot(2));
}

TEST_F(D3D11FrameRingTest, MatchesChecksSizeFormatAndPublisher) {
    D3D11FrameRing ring;
    EXPECT_FALSE(ring.Matches(0, 0, DXGI_FORMAT_UNKNOWN)) << "not created yet";
    ASSERT_TRUE(ring.Create(warp.device.Get(), 64, 32, DXGI_FORMAT_R8G8B8A8_UNORM, kApiOpenGL));
    EXPECT_TRUE(ring.Matches(64, 32, DXGI_FORMAT_R8G8B8A8_UNORM));
    EXPECT_FALSE(ring.Matches(65, 32, DXGI_FORMAT_R8G8B8A8_UNORM));
    EXPECT_FALSE(ring.Matches(64, 33, DXGI_FORMAT_R8G8B8A8_UNORM));
    EXPECT_FALSE(ring.Matches(64, 32, DXGI_FORMAT_B8G8R8A8_UNORM));

    D3D11FrameRing other;
    ASSERT_TRUE(other.Create(warp.device.Get(), 64, 32, DXGI_FORMAT_R8G8B8A8_UNORM, kApiD3D11));
    EXPECT_FALSE(ring.Matches(64, 32, DXGI_FORMAT_R8G8B8A8_UNORM)) << "another ring took over";
    EXPECT_TRUE(other.Matches(64, 32, DXGI_FORMAT_R8G8B8A8_UNORM));
}

TEST_F(D3D11FrameRingTest, RecreateBumpsGeneration) {
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
    ASSERT_TRUE(ring.Create(warp.device.Get(), 16, 16, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
    EXPECT_EQ(ring.Generation(), 2u);
    EXPECT_EQ(host->texSeq, 4);
    EXPECT_EQ(host->width, 16u);
}

TEST_F(D3D11FrameRingTest, ReleaseDropsResources) {
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
    ring.Release();
    EXPECT_FALSE(ring.Matches(8, 8, DXGI_FORMAT_B8G8R8A8_UNORM));
    EXPECT_EQ(ring.Width(), 0u);
    EXPECT_EQ(ring.Format(), DXGI_FORMAT_UNKNOWN);
    EXPECT_EQ(ring.SlotHandle(0), nullptr);
    EXPECT_EQ(ring.FrameFenceHandle(), nullptr);
}

TEST_F(D3D11FrameRingTest, CreateFailsForUnsupportedFormat) {
    D3D11FrameRing ring;
    EXPECT_FALSE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_UNKNOWN, kApiD3D11));
    EXPECT_EQ(host->texSeq, 0) << "nothing published";
    EXPECT_EQ(std::string(host->status).rfind("D3D11 shared texture 8x8", 0), 0u) << host->status;
    EXPECT_EQ(ring.SlotHandle(0), nullptr);
}

TEST_F(D3D11FrameRingTest, FencesStartAtCurrentFrame) {
    host->frameIndex = 10;
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));
    EXPECT_EQ(host->firstFrame, 11);

    // The host fence starts at 10, so the host reading frame 10 doesn't block slot 10 + N.
    host->hostReadingFrame = 10;
    EXPECT_TRUE(ring.Claim(10 + kFrameSlots));
    host->hostReadingFrame = 11;
    EXPECT_FALSE(ring.Claim(11 + kFrameSlots)) << "host fence hasn't reached 11";
}

TEST_F(D3D11FrameRingTest, PublishSignalsFrameFenceAndHost) {
    D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM, kApiD3D11));

    // Open the frame fence the way the host would, to watch the signal arrive.
    Microsoft::WRL::ComPtr<ID3D11Fence> fence;
    ASSERT_HRESULT_SUCCEEDED(
        warp.device->OpenSharedFence(ring.FrameFenceHandle(), IID_PPV_ARGS(&fence)));

    ASSERT_TRUE(ring.Claim(1));
    ring.Publish(warp.context.Get(), 1, 777, true);
    EXPECT_EQ(host->frameIndex, 1);
    EXPECT_EQ(host->lastPresentQpc, 777);
    EXPECT_EQ(host->hookState, kHookCapturing);
    EXPECT_EQ(WaitForSingleObject(FakeHost::Get().FrameEvent(), 0), WAIT_OBJECT_0);

    HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_HRESULT_SUCCEEDED(fence->SetEventOnCompletion(1, done));
    EXPECT_EQ(WaitForSingleObject(done, 5000), WAIT_OBJECT_0);
    CloseHandle(done);
}

}  // namespace
}  // namespace gazeora::hook
