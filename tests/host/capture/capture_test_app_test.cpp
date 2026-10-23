// The test app captured as Gazeora captures a game: launched with the real hook, its frames
// read through the shared block and textures. The test app and hook of this build, so the x86
// release tests 32-bit capture and the x64 release 64-bit capture.
#include <windows.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/host/capture/capture_client.h"
#include "gazeora/host/launch/injector.h"
#include "gazeora/host/launch/launcher.h"

namespace gazeora {
namespace {

constexpr DWORD kFirstFrameTimeoutMs = 15000;
constexpr DWORD kWaitStepMs = 100;

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    return full.substr(0, full.find_last_of(L"\\/") + 1);
}

struct Api {
    const wchar_t* flag;
    uint32_t api;
    const char* name;
};

class CaptureTestAppTest : public ::testing::TestWithParam<Api> {};

TEST_P(CaptureTestAppTest, CapturesFramesFromTheTestApp) {
    std::wstring testApp = ExeDir() + L"gazeora_testapp.exe";
    if (GetFileAttributesW(testApp.c_str()) == INVALID_FILE_ATTRIBUTES) {
        GTEST_SKIP() << "gazeora_testapp.exe must be next to the tests";
    }
    LaunchSpec spec;
    spec.exePath = testApp;
    spec.arguments = GetParam().flag;
    CaptureClient client;
    LaunchedGame game;
    std::wstring error;
    ASSERT_TRUE(LaunchGameWithHook(spec, HookDllPathNextToExe(), client, game, error))
        << std::string(error.begin(), error.end());
    client.SetCaptureEnabled(true);
    bool gotFrame = false;
    for (DWORD waited = 0; !gotFrame && waited < kFirstFrameTimeoutMs && game.Running();
         waited += kWaitStepMs) {
        gotFrame = client.WaitForFrame(kWaitStepMs) == CaptureClient::WaitResult::Frame;
    }
    const SharedHeader& shared = client.Shared();
    EXPECT_TRUE(gotFrame) << "hook: " << shared.status;
    EXPECT_EQ(shared.api, GetParam().api);
    EXPECT_EQ(shared.width, 1280u);
    EXPECT_EQ(shared.height, 720u);
    EXPECT_NE(client.Frame(), nullptr);
    TerminateProcess(game.process, 0);
    WaitForSingleObject(game.process, 5000);
}

// D3D9 for every build; D3D8 where there is one, in 32-bit builds whose test app has it
// (GAZEORA_TEST_D3D8, tests/CMakeLists.txt): Windows' D3D8 is 32-bit only.
std::vector<Api> Apis() {
    std::vector<Api> apis = {{L"--d3d9", kApiD3D9, "D3D9"}};
#ifdef GAZEORA_TEST_D3D8
    apis.push_back({L"--d3d8", kApiD3D8, "D3D8"});
#endif
    return apis;
}

INSTANTIATE_TEST_SUITE_P(Apis, CaptureTestAppTest, ::testing::ValuesIn(Apis()),
                         [](const auto& info) { return std::string(info.param.name); });

}  // namespace
}  // namespace gazeora
