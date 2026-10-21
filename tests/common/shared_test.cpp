#include "gazeora/common/shared.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace gazeora {
namespace {

TEST(SharedProtocol, GraphicsApiName) {
    EXPECT_STREQ(GraphicsApiName(kApiD3D8), "D3D8");
    EXPECT_STREQ(GraphicsApiName(kApiD3D9), "D3D9");
    EXPECT_STREQ(GraphicsApiName(kApiD3D10), "D3D10");
    EXPECT_STREQ(GraphicsApiName(kApiD3D11), "D3D11");
    EXPECT_STREQ(GraphicsApiName(kApiD3D12), "D3D12");
    EXPECT_STREQ(GraphicsApiName(kApiOpenGL), "OpenGL");
    EXPECT_STREQ(GraphicsApiName(kApiVulkan), "Vulkan");
    EXPECT_STREQ(GraphicsApiName(kApiUnknown), "unknown API");
    EXPECT_STREQ(GraphicsApiName(42), "unknown API");
}

TEST(SharedProtocol, ObjectNamesArePerProcess) {
    EXPECT_EQ(SharedMemoryName(1234), L"Local\\Gazeora_shm_1234");
    EXPECT_EQ(FrameEventName(1234), L"Local\\Gazeora_frame_1234");
    EXPECT_EQ(SharedMemoryName(4294967295u), L"Local\\Gazeora_shm_4294967295");
    EXPECT_NE(SharedMemoryName(1), SharedMemoryName(2));
}

TEST(SharedProtocol, MagicSpellsGaza) {
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(&kSharedMagic), 4), "GAZA");
}

// The block is shared between separately built processes, so its fields that are
// accessed with interlocked operations must stay naturally aligned.
TEST(SharedProtocol, InterlockedFieldsAreAligned) {
    EXPECT_EQ(offsetof(SharedHeader, hookState) % sizeof(LONG), 0u);
    EXPECT_EQ(offsetof(SharedHeader, captureEnabled) % sizeof(LONG), 0u);
    EXPECT_EQ(offsetof(SharedHeader, texSeq) % sizeof(LONG), 0u);
    EXPECT_EQ(offsetof(SharedHeader, frameIndex) % 8, 0u);
    EXPECT_EQ(offsetof(SharedHeader, writingFrame) % 8, 0u);
    EXPECT_EQ(offsetof(SharedHeader, hostReadingFrame) % 8, 0u);
    EXPECT_EQ(offsetof(SharedHeader, lastPresentQpc) % 8, 0u);
    EXPECT_EQ(offsetof(SharedHeader, skippedFrames) % 8, 0u);
}

TEST(SharedProtocol, StatusIsLastField) {
    EXPECT_EQ(offsetof(SharedHeader, status) + sizeof(SharedHeader::status), sizeof(SharedHeader));
}

}  // namespace
}  // namespace gazeora
