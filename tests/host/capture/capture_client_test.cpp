#include "gazeora/host/capture/capture_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace gazeora {
namespace {

// The tests use their own process as the "game".
DWORD Self() { return GetCurrentProcessId(); }

// A second view of the shared block, standing in for the hook.
class HookView {
public:
    HookView() {
        m_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SharedMemoryName(Self()).c_str());
        if (m_mapping) {
            m_shared = static_cast<SharedHeader*>(
                MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)));
        }
    }
    ~HookView() {
        if (m_shared) UnmapViewOfFile(m_shared);
        if (m_mapping) CloseHandle(m_mapping);
    }
    HookView(const HookView&) = delete;
    HookView& operator=(const HookView&) = delete;

    SharedHeader* operator->() const { return m_shared; }
    explicit operator bool() const { return m_shared != nullptr; }

private:
    HANDLE m_mapping = nullptr;
    SharedHeader* m_shared = nullptr;
};

TEST(CaptureClient, CreateInitializesSharedBlock) {
    CaptureClient client;
    std::wstring error;
    ASSERT_TRUE(client.Create(Self(), error)) << error;
    EXPECT_EQ(client.Shared().magic, kSharedMagic);
    EXPECT_EQ(client.Shared().version, kSharedVersion);
    EXPECT_EQ(client.Shared().hookState, kHookNotLoaded);
    EXPECT_EQ(client.Shared().captureEnabled, 0);
    EXPECT_EQ(client.Shared().texSeq, 0);
    EXPECT_FALSE(client.HookAlreadyLoaded());
    EXPECT_EQ(client.Frame(), nullptr);
    EXPECT_EQ(client.Device(), nullptr);
}

TEST(CaptureClient, CreatesNamedFrameEvent) {
    CaptureClient client;
    std::wstring error;
    ASSERT_TRUE(client.Create(Self(), error)) << error;
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, FrameEventName(Self()).c_str());
    EXPECT_NE(event, nullptr) << "the hook opens the event by name";
    if (event) CloseHandle(event);
}

TEST(CaptureClient, CreateFailsForMissingProcess) {
    CaptureClient client;
    std::wstring error;
    EXPECT_FALSE(client.Create(0, error));
    EXPECT_EQ(error.rfind(L"OpenProcess(PROCESS_DUP_HANDLE) failed", 0), 0u) << error;
}

TEST(CaptureClient, SetCaptureEnabledIsVisibleToHook) {
    CaptureClient client;
    std::wstring error;
    ASSERT_TRUE(client.Create(Self(), error)) << error;
    HookView hook;
    ASSERT_TRUE(hook);
    client.SetCaptureEnabled(true);
    EXPECT_EQ(hook->captureEnabled, 1);
    client.SetCaptureEnabled(false);
    EXPECT_EQ(hook->captureEnabled, 0);
}

TEST(CaptureClient, DestructorStopsCaptureAndReleasesSlot) {
    auto client = std::make_unique<CaptureClient>();
    std::wstring error;
    ASSERT_TRUE(client->Create(Self(), error)) << error;
    HookView hook;  // keeps the block alive, like the hook does
    ASSERT_TRUE(hook);
    client->SetCaptureEnabled(true);
    hook->hostReadingFrame = 5;
    client.reset();
    EXPECT_EQ(hook->captureEnabled, 0);
    EXPECT_EQ(hook->hostReadingFrame, 0) << "the hook must not wait for a host that left";
}

TEST(CaptureClient, ReattachesToRunningHook) {
    auto first = std::make_unique<CaptureClient>();
    std::wstring error;
    ASSERT_TRUE(first->Create(Self(), error)) << error;
    HookView hook;
    ASSERT_TRUE(hook);
    hook->hookState = kHookCapturing;
    hook->frameIndex = 77;
    first.reset();  // host restarts while the hook keeps the block open

    CaptureClient second;
    ASSERT_TRUE(second.Create(Self(), error)) << error;
    EXPECT_TRUE(second.HookAlreadyLoaded());
    EXPECT_EQ(second.Shared().frameIndex, 77) << "existing state kept";
}

TEST(CaptureClient, ReattachIgnoresBlockWithoutHook) {
    auto first = std::make_unique<CaptureClient>();
    std::wstring error;
    ASSERT_TRUE(first->Create(Self(), error)) << error;
    HookView hook;
    first.reset();

    CaptureClient second;
    ASSERT_TRUE(second.Create(Self(), error)) << error;
    EXPECT_FALSE(second.HookAlreadyLoaded()) << "hookState is still kHookNotLoaded";
}

TEST(CaptureClient, ReinitializesBlockWithWrongMagic) {
    auto first = std::make_unique<CaptureClient>();
    std::wstring error;
    ASSERT_TRUE(first->Create(Self(), error)) << error;
    HookView hook;
    hook->magic = 0;
    hook->hookState = kHookCapturing;
    hook->frameIndex = 77;
    first.reset();

    CaptureClient second;
    ASSERT_TRUE(second.Create(Self(), error)) << error;
    EXPECT_FALSE(second.HookAlreadyLoaded());
    EXPECT_EQ(second.Shared().magic, kSharedMagic);
    EXPECT_EQ(second.Shared().frameIndex, 0) << "block zeroed";
}

TEST(CaptureClient, RefusesHookWithOtherProtocolVersion) {
    auto first = std::make_unique<CaptureClient>();
    std::wstring error;
    ASSERT_TRUE(first->Create(Self(), error)) << error;
    HookView hook;
    hook->version = kSharedVersion - 1;
    hook->hookState = kHookCapturing;
    first.reset();

    CaptureClient second;
    EXPECT_FALSE(second.Create(Self(), error));
    EXPECT_EQ(error,
              L"an older Gazeora hook is already loaded in this game; restart the game first");
}

TEST(CaptureClient, WaitForFrameTimesOutWithoutHook) {
    CaptureClient client;
    std::wstring error;
    ASSERT_TRUE(client.Create(Self(), error)) << error;
    EXPECT_EQ(client.WaitForFrame(10), CaptureClient::WaitResult::Timeout);
}

TEST(CaptureClient, WaitForFrameTimesOutBeforeResourcesArePublished) {
    CaptureClient client;
    std::wstring error;
    ASSERT_TRUE(client.Create(Self(), error)) << error;
    HookView hook;
    hook->frameIndex = 1;
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, FrameEventName(Self()).c_str());
    ASSERT_NE(event, nullptr);
    SetEvent(event);
    CloseHandle(event);
    EXPECT_EQ(client.WaitForFrame(10), CaptureClient::WaitResult::Timeout) << "texSeq is 0";
    EXPECT_EQ(hook->hostReadingFrame, 0);
}

}  // namespace
}  // namespace gazeora
