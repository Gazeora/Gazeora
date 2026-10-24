#include "gazeora/host/launch/launcher.h"

#include <gtest/gtest.h>

#include "gazeora/host/launch/injector.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gazeora {
namespace {

std::wstring SystemExe(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name;
}

TEST(Launcher, LaunchGamePassesArgumentsAndWorkingDir) {
    // cmd exits with the code passed on its command line.
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    LaunchSpec spec;
    spec.exePath = SystemExe(L"cmd.exe");
    spec.arguments = L"/c exit 7";
    spec.workingDir = temp;

    LaunchedGame game;
    std::wstring error;
    ASSERT_TRUE(LaunchGame(spec, game, error)) << error;
    EXPECT_NE(game.pid, 0u);
    ASSERT_EQ(WaitForSingleObject(game.process, 10000), WAIT_OBJECT_0);
    EXPECT_FALSE(game.Running());
    DWORD exitCode = 0;
    GetExitCodeProcess(game.process, &exitCode);
    EXPECT_EQ(exitCode, 7u);
}

// A launcher stub: exits at once and never shows a window.
TEST(HandOffDetector, EarlyExitWithoutAWindowIsAHandOff) {
    std::wstring command = L"\"" + SystemExe(L"cmd.exe") + L"\" /c exit 0";
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION info = {};
    // No console window: Windows reports a console as owned by its client process.
    ASSERT_TRUE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &startup, &info));
    CloseHandle(info.hThread);
    HandOffDetector detector(info.dwProcessId, info.hProcess);
    ASSERT_EQ(WaitForSingleObject(info.hProcess, 10000), WAIT_OBJECT_0);
    EXPECT_FALSE(detector.ShowedWindow());
    EXPECT_TRUE(detector.HandedOff());
    CloseHandle(info.hProcess);
}

// A game: shows a window, so closing it soon after isn't a hand-off. Watched on this
// process, with a window placed off-screen.
TEST(HandOffDetector, AProcessThatShowedAWindowWasPlayed) {
    HandOffDetector detector(GetCurrentProcessId(), GetCurrentProcess());
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"game",
                                  WS_POPUP | WS_VISIBLE, -32000, -32000, 16, 16, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(window, nullptr);
    for (int i = 0; i < 50 && !detector.ShowedWindow(); ++i) Sleep(20);
    EXPECT_TRUE(detector.ShowedWindow());
    EXPECT_FALSE(detector.HandedOff());
    DestroyWindow(window);
}

TEST(Launcher, LaunchGameReportsMissingExe) {
    LaunchSpec spec;
    spec.exePath = L"C:\\gazeora\\no_such_game.exe";
    LaunchedGame game;
    std::wstring error;
    EXPECT_FALSE(LaunchGame(spec, game, error));
    EXPECT_EQ(error.rfind(L"game executable not found", 0), 0u);
    EXPECT_EQ(game.process, nullptr);
}

TEST(Launcher, LaunchGameWithHookReportsMissingDll) {
    LaunchSpec spec;
    spec.exePath = SystemExe(L"cmd.exe");
    CaptureClient client;
    LaunchedGame game;
    std::wstring error;
    EXPECT_FALSE(LaunchGameWithHook(spec, L"C:\\gazeora\\no_such.dll", client, game, error));
    EXPECT_EQ(error.rfind(L"hook DLL not found", 0), 0u);
    EXPECT_EQ(game.process, nullptr);
}

TEST(Launcher, LaunchedGameMoveTransfersOwnership) {
    LaunchSpec spec;
    spec.exePath = SystemExe(L"cmd.exe");
    spec.arguments = L"/c exit 0";
    LaunchedGame first;
    std::wstring error;
    ASSERT_TRUE(LaunchGame(spec, first, error)) << error;
    HANDLE process = first.process;

    LaunchedGame second = std::move(first);
    EXPECT_EQ(first.process, nullptr);
    EXPECT_EQ(first.pid, 0u);
    EXPECT_EQ(second.process, process);
    WaitForSingleObject(second.process, 10000);
}

class SteamLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = std::wstring(temp) + L"gazeora_steam_test_" + std::to_wstring(GetCurrentProcessId());
        steamapps = root + L"\\SteamApps";  // Steam libraries don't agree on the case
        CreateDirectoryW(root.c_str(), nullptr);
        CreateDirectoryW(steamapps.c_str(), nullptr);
    }

    void TearDown() override {
        for (const std::wstring& file : files) DeleteFileW(file.c_str());
        RemoveDirectoryW(steamapps.c_str());
        RemoveDirectoryW(root.c_str());
    }

    void WriteManifest(const char* appId, const char* installDir) {
        std::wstring path =
            steamapps + L"\\appmanifest_" + std::wstring(appId, appId + strlen(appId)) + L".acf";
        FILE* f = _wfopen(path.c_str(), L"wb");
        ASSERT_NE(f, nullptr);
        fprintf(f,
                "\"AppState\"\n{\n\t\"appid\"\t\t\"%s\"\n\t\"name\"\t\t\"Some Game\"\n"
                "\t\"installdir\"\t\t\"%s\"\n}\n",
                appId, installDir);
        fclose(f);
        files.push_back(path);
    }

    std::wstring root, steamapps;
    std::vector<std::wstring> files;
};

TEST_F(SteamLibraryTest, FindsAppIdFromManifestInstallDir) {
    WriteManifest("100", "Other Game");
    WriteManifest("2208920", "Assassin's Creed Valhalla");
    std::wstring exe = steamapps + L"\\common\\Assassin's Creed Valhalla\\bin\\ACValhalla.exe";
    EXPECT_EQ(FindSteamAppId(exe), L"2208920");
}

TEST_F(SteamLibraryTest, InstallDirMatchIsCaseInsensitive) {
    WriteManifest("42", "My Game");
    EXPECT_EQ(FindSteamAppId(steamapps + L"\\common\\MY GAME\\game.exe"), L"42");
}

TEST_F(SteamLibraryTest, NoAppIdOutsideSteamOrWithoutManifest) {
    WriteManifest("42", "My Game");
    EXPECT_EQ(FindSteamAppId(steamapps + L"\\common\\Unknown Game\\game.exe"), L"");
    EXPECT_EQ(FindSteamAppId(L"C:\\Games\\My Game\\game.exe"), L"");
}

TEST(Launcher, GameProcessNameDefaultsToExeName) {
    LaunchSpec spec;
    spec.exePath = L"C:\\Games\\Foo\\Launcher.exe";
    EXPECT_EQ(spec.GameProcessName(), L"Launcher.exe");
    spec.processName = L"FooGame.exe";
    EXPECT_EQ(spec.GameProcessName(), L"FooGame.exe");
}

// A process that runs a few seconds and never loads a graphics runtime.
std::wstring StartPing(LaunchedGame& ping) {
    LaunchSpec spec;
    spec.exePath = SystemExe(L"PING.EXE");
    spec.arguments = L"-n 6 127.0.0.1";
    std::wstring error;
    EXPECT_TRUE(LaunchGame(spec, ping, error)) << error;
    return L"PING.EXE";
}

TEST(Launcher, WaitForGameProcessFindsProcessByName) {
    LaunchedGame ping;
    std::wstring name = StartPing(ping);
    LaunchedGame found;
    std::wstring error;
    // Short ready timeout: ping never shows a window, so it's taken once that passes.
    ASSERT_TRUE(WaitForGameProcess(name, 0, 5000, 300, nullptr, found, error)) << error;
    EXPECT_NE(found.pid, 0u);
    EXPECT_NE(found.process, nullptr);
    TerminateProcess(ping.process, 0);
}

TEST(Launcher, WaitForGameProcessIgnoresGivenPid) {
    LaunchedGame ping;
    std::wstring name = StartPing(ping);
    if (FindProcessByName(name, ping.pid) != 0) GTEST_SKIP() << "another ping is running";
    LaunchedGame found;
    std::wstring error;
    EXPECT_FALSE(WaitForGameProcess(name, ping.pid, 600, 300, nullptr, found, error));
    EXPECT_EQ(error.rfind(L"timed out", 0), 0u);
    TerminateProcess(ping.process, 0);
}

TEST(Launcher, WaitForGameProcessCanBeCancelled) {
    LaunchedGame found;
    std::wstring error;
    EXPECT_FALSE(WaitForGameProcess(
        L"gazeora_no_such_game_7a2e.exe", 0, 60000, 300, [] { return true; }, found, error));
    EXPECT_EQ(error, L"cancelled");
}

TEST(Launcher, AttachHookReportsMissingDll) {
    CaptureClient client;
    std::wstring error;
    EXPECT_FALSE(AttachHook(GetCurrentProcessId(), L"C:\\gazeora\\no_such.dll", client, error));
    EXPECT_EQ(error.rfind(L"hook DLL not found", 0), 0u);
}

}  // namespace
}  // namespace gazeora
