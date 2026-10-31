// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Gazeora capture hook. Injected into the game process by Gazeora.exe.
//
// Each graphics API lives in its own folder and exposes an Install() function that
// creates its MinHook detours. They all hand frames to the API-agnostic transport in
// core/shared_channel (protocol in common/shared.h). To support a new API, add a
// folder with its hooks and capture backend and register it in kModules below.

#include <windows.h>

#include <MinHook.h>

#include <string>

#include "gazeora/hook/core/log.h"
#include "gazeora/hook/core/shared_channel.h"
#include "gazeora/hook/d3d12/d3d12_queue_tracker.h"
#include "gazeora/hook/d3d8/d3d8_hooks.h"
#include "gazeora/hook/d3d9/d3d9_hooks.h"
#include "gazeora/hook/dxgi/dxgi_hooks.h"
#include "gazeora/hook/opengl/gl_hooks.h"

namespace gazeora::hook {

namespace {

struct HookModule {
    const char* name;
    bool (*install)();
    bool required;  // fail the whole hook if this one can't be installed
};

// Modules for APIs other than DXGI only install when the game uses that API (or,
// for games the host launched, defer their hooks until the game initializes it).
const HookModule kModules[] = {
    {"DXGI (D3D10/D3D11/D3D12)", dxgi::Install, true},
    {"D3D12 queues", d3d12::InstallQueueTracker, false},
    {"D3D9", d3d9::Install, false},
    {"D3D8", d3d8::Install, false},
    {"OpenGL", opengl::Install, false},
};

DWORD WINAPI InitThread(LPVOID) {
    // Loaded by something other than Gazeora.exe: stay dormant.
    if (!OpenSharedChannel()) {
        OutputDebugStringA("[gazeora_hook] no host shared memory, not hooking\n");
        return 0;
    }
    SetHookState(kHookInitializing);

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        SetStatus("MinHook init failed: %s", MH_StatusToString(status));
        SetHookState(kHookError);
        return 0;
    }

    std::string installed;
    for (const HookModule& module : kModules) {
        if (module.install()) {
            installed += installed.empty() ? "" : ", ";
            installed += module.name;
        } else if (module.required) {
            SetHookState(kHookError);  // the module already reported why
            return 0;
        }
    }

    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        SetStatus("enabling hooks failed: %s", MH_StatusToString(status));
        SetHookState(kHookError);
        return 0;
    }

    SetStatus("hooked %s, waiting for frames", installed.c_str());
    SetHookState(kHookInstalled);
    return 0;
}

}  // namespace

}  // namespace gazeora::hook

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        // Never unload: a Present call could be executing our code at any moment.
        HMODULE self;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &self);
        HANDLE thread = CreateThread(nullptr, 0, gazeora::hook::InitThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
