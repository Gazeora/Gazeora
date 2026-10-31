// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Utilities shared by the per-API hook modules.
#pragma once

#include <windows.h>

namespace gazeora::hook {

inline void* VTableEntry(void* object, size_t slot) {
    return (*static_cast<void***>(object))[slot];
}

// Creates a MinHook detour (enabled later, all at once). Logs and returns false on failure.
bool CreateHookRaw(const char* name, void* target, void* detour, void** original);

template <typename Fn>
bool CreateHook(const char* name, void* target, Fn detour, Fn* original) {
    return CreateHookRaw(name, target, reinterpret_cast<void*>(detour),
                         reinterpret_cast<void**>(original));
}

// Creates and immediately enables a detour, for hooks installed after startup (e.g.
// once the game initializes a graphics API). Logs and returns false on failure.
bool CreateEnabledHookRaw(const char* name, void* target, void* detour, void** original);

template <typename Fn>
bool CreateEnabledHook(const char* name, void* target, Fn detour, Fn* original) {
    return CreateEnabledHookRaw(name, target, reinterpret_cast<void*>(detour),
                                reinterpret_cast<void**>(original));
}

// Address of an export of an already loaded module, or nullptr.
void* ModuleExport(HMODULE module, const char* name);

// Returns `dll` if the game already loaded it. Otherwise, if the host launched the game
// (so the hook runs before game code), loads it so it can be hooked before first use.
HMODULE GraphicsRuntime(const wchar_t* dll);

// Hidden window for creating throwaway devices/swapchains to read vtables from.
class DummyWindow {
public:
    DummyWindow();
    ~DummyWindow();
    DummyWindow(const DummyWindow&) = delete;
    DummyWindow& operator=(const DummyWindow&) = delete;

    HWND hwnd() const { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
};

LONGLONG QpcNow();
LONGLONG QpcFrequency();

}  // namespace gazeora::hook
