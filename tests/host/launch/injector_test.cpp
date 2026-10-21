#include "gazeora/host/launch/injector.h"

#include <gtest/gtest.h>

#include <cwctype>
#include <string>

namespace gazeora {
namespace {

std::wstring OwnExeName() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    return full.substr(full.find_last_of(L"\\/") + 1);
}

std::wstring SystemDll(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name;
}

TEST(Injector, FindProcessByNameFindsRunningProcess) {
    // ctest may run several copies of this executable at once, so any of them will do.
    EXPECT_NE(FindProcessByName(OwnExeName()), 0u);
}

TEST(Injector, FindProcessByNameIgnoresCase) {
    std::wstring upper = OwnExeName();
    for (wchar_t& c : upper) c = wchar_t(towupper(c));
    EXPECT_NE(FindProcessByName(upper), 0u);
}

TEST(Injector, FindProcessByNameReturnsZeroWhenMissing) {
    EXPECT_EQ(FindProcessByName(L"gazeora_no_such_process_6f1c.exe"), 0u);
    EXPECT_EQ(FindProcessByName(L""), 0u);
}

TEST(Injector, InjectDllReportsOpenProcessFailure) {
    std::wstring error;
    EXPECT_FALSE(InjectDll(0, SystemDll(L"version.dll"), error));
    EXPECT_EQ(error.rfind(L"OpenProcess failed", 0), 0u);
}

TEST(Injector, InjectDllLoadsDllIntoTarget) {
    // Inject into ourselves, the one process the test is sure to have access to.
    const wchar_t* dll = L"d3d10.dll";
    if (GetModuleHandleW(dll)) GTEST_SKIP() << "already loaded";
    std::wstring error;
    ASSERT_TRUE(InjectDll(GetCurrentProcessId(), SystemDll(dll), error)) << error;
    HMODULE module = GetModuleHandleW(dll);
    EXPECT_NE(module, nullptr);
    FreeLibrary(module);
}

TEST(Injector, InjectDllReportsLoadLibraryFailure) {
    std::wstring error;
    EXPECT_FALSE(InjectDll(GetCurrentProcessId(), L"C:\\gazeora\\no_such.dll", error));
    EXPECT_EQ(error, L"LoadLibraryW failed inside the target (missing DLL dependency?)");
}

}  // namespace
}  // namespace gazeora
