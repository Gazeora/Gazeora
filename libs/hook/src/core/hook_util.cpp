// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/hook_util.h"

#include <MinHook.h>

#include "gazeora/hook/core/log.h"
#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook {

namespace {
constexpr wchar_t kDummyClass[] = L"GazeoraDummyWindow";
}  // namespace

bool CreateHookRaw(const char* name, void* target, void* detour, void** original) {
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        SetStatus("hooking %s failed: %s", name, MH_StatusToString(status));
        return false;
    }
    return true;
}

bool CreateEnabledHookRaw(const char* name, void* target, void* detour, void** original) {
    if (!CreateHookRaw(name, target, detour, original)) {
        return false;
    }
    MH_STATUS status = MH_EnableHook(target);
    if (status != MH_OK) {
        SetStatus("enabling %s hook failed: %s", name, MH_StatusToString(status));
        MH_RemoveHook(target);
        return false;
    }
    return true;
}

void* ModuleExport(HMODULE module, const char* name) {
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

HMODULE GraphicsRuntime(const wchar_t* dll) {
    HMODULE module = GetModuleHandleW(dll);
    if (!module && LaunchedByHost()) {
        module = LoadLibraryW(dll);
    }
    return module;
}

DummyWindow::DummyWindow() {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kDummyClass;
    RegisterClassExW(&wc);
    m_hwnd = CreateWindowExW(0, kDummyClass, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, nullptr,
                             nullptr, wc.hInstance, nullptr);
    if (!m_hwnd) {
        SetStatus("CreateWindowEx for dummy window failed: %lu", GetLastError());
    }
}

DummyWindow::~DummyWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    UnregisterClassW(kDummyClass, GetModuleHandleW(nullptr));
}

LONGLONG QpcNow() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

LONGLONG QpcFrequency() {
    static const LONGLONG freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    return freq;
}

}  // namespace gazeora::hook
