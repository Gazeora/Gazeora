// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/hook_util.h"

#include <MinHook.h>
#include <gtest/gtest.h>

#include <string>

#include "hook/fake_host.h"

namespace gazeora::hook {
namespace {

using test::FakeHost;

// A runtime the test process never loads on its own.
constexpr wchar_t kUnloadedDll[] = L"d3d10.dll";

class HookUtilTest : public ::testing::Test {
protected:
    void SetUp() override { FakeHost::Get().Reset(); }
};

struct FakeComObject {
    void* const* vtable;
};

TEST_F(HookUtilTest, VTableEntryReadsSlot) {
    int a = 0, b = 0, c = 0;
    void* const vtable[] = {&a, &b, &c};
    FakeComObject object = {vtable};
    EXPECT_EQ(VTableEntry(&object, 0), &a);
    EXPECT_EQ(VTableEntry(&object, 2), &c);
}

TEST_F(HookUtilTest, ModuleExport) {
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    EXPECT_EQ(ModuleExport(kernel32, "GetCurrentProcessId"),
              reinterpret_cast<void*>(GetProcAddress(kernel32, "GetCurrentProcessId")));
    EXPECT_EQ(ModuleExport(kernel32, "NoSuchExportInKernel32"), nullptr);
    EXPECT_EQ(ModuleExport(nullptr, "GetCurrentProcessId"), nullptr);
}

TEST_F(HookUtilTest, GraphicsRuntimeReturnsLoadedModule) {
    EXPECT_EQ(GraphicsRuntime(L"kernel32.dll"), GetModuleHandleW(L"kernel32.dll"));
}

TEST_F(HookUtilTest, GraphicsRuntimeDoesNotLoadIntoAttachedGames) {
    if (GetModuleHandleW(kUnloadedDll)) GTEST_SKIP() << "already loaded";
    EXPECT_EQ(GraphicsRuntime(kUnloadedDll), nullptr);
    EXPECT_EQ(GetModuleHandleW(kUnloadedDll), nullptr);
}

TEST_F(HookUtilTest, GraphicsRuntimeLoadsIntoGamesTheHostLaunched) {
    if (GetModuleHandleW(kUnloadedDll)) GTEST_SKIP() << "already loaded";
    FakeHost::Get().Shared()->hostFlags = kHostLaunchedGame;
    HMODULE module = GraphicsRuntime(kUnloadedDll);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(GetModuleHandleW(kUnloadedDll), module);
    FreeLibrary(module);
}

TEST_F(HookUtilTest, DummyWindowLifetime) {
    HWND hwnd;
    {
        DummyWindow window;
        hwnd = window.hwnd();
        ASSERT_NE(hwnd, nullptr);
        EXPECT_TRUE(IsWindow(hwnd));
        EXPECT_FALSE(IsWindowVisible(hwnd));
    }
    EXPECT_FALSE(IsWindow(hwnd));
    DummyWindow again;  // the class can be registered again
    EXPECT_NE(again.hwnd(), nullptr);
}

TEST_F(HookUtilTest, Qpc) {
    EXPECT_GT(QpcFrequency(), 0);
    EXPECT_EQ(QpcFrequency(), QpcFrequency());
    LONGLONG first = QpcNow();
    EXPECT_LE(first, QpcNow());
}

// Target for the detour tests. Called through a volatile pointer so it isn't inlined.
__declspec(noinline) int Original(int x) { return x * 2 + 1; }
int (*volatile g_original)(int) = Original;

// MinHook takes targets as void*; function pointers only convert to it explicitly.
void* OriginalTarget() { return reinterpret_cast<void*>(g_original); }

int (*g_trampoline)(int) = nullptr;
int Detour(int x) { return g_trampoline(x) + 1000; }

TEST_F(HookUtilTest, CreateHookReportsFailure) {
    int (*trampoline)(int) = nullptr;
    EXPECT_FALSE(CreateHook("Nothing", nullptr, Detour, &trampoline));
    EXPECT_EQ(std::string(FakeHost::Get().Shared()->status).rfind("hooking Nothing failed: ", 0),
              0u)
        << FakeHost::Get().Shared()->status;
}

TEST_F(HookUtilTest, CreateEnabledHookDetoursImmediately) {
    ASSERT_EQ(MH_Initialize(), MH_OK);
    ASSERT_TRUE(CreateEnabledHook("Original", OriginalTarget(), Detour, &g_trampoline));
    EXPECT_EQ(g_original(5), 1011) << "detour runs and calls the original";
    EXPECT_EQ(MH_Uninitialize(), MH_OK);
    EXPECT_EQ(g_original(5), 11) << "unhooked again";
}

TEST_F(HookUtilTest, CreateHookWaitsToBeEnabled) {
    ASSERT_EQ(MH_Initialize(), MH_OK);
    ASSERT_TRUE(CreateHook("Original", OriginalTarget(), Detour, &g_trampoline));
    EXPECT_EQ(g_original(5), 11);
    ASSERT_EQ(MH_EnableHook(MH_ALL_HOOKS), MH_OK);
    EXPECT_EQ(g_original(5), 1011);
    EXPECT_EQ(MH_Uninitialize(), MH_OK);
}

TEST_F(HookUtilTest, CreateEnabledHookReportsDuplicateHook) {
    ASSERT_EQ(MH_Initialize(), MH_OK);
    ASSERT_TRUE(CreateEnabledHook("Original", OriginalTarget(), Detour, &g_trampoline));
    int (*second)(int) = nullptr;
    EXPECT_FALSE(CreateEnabledHook("Original again", OriginalTarget(), Detour, &second));
    EXPECT_STREQ(FakeHost::Get().Shared()->status,
                 "hooking Original again failed: MH_ERROR_ALREADY_CREATED");
    EXPECT_EQ(MH_Uninitialize(), MH_OK);
}

}  // namespace
}  // namespace gazeora::hook
