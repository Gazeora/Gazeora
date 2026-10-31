// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/controller_hooks.h"

#include <windows.h>
#include <xinput.h>

#include <cwchar>
#include <string>

#include "gazeora/common/shared.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/log.h"
#include "gazeora/hook/input/head_turn_feed.h"
#include "gazeora/hook/input/pad_reports.h"

namespace gazeora::hook::input {

namespace {

using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
using CloseHandleFn = BOOL(WINAPI*)(HANDLE);
using ReadFileFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using GetOverlappedResultFn = BOOL(WINAPI*)(HANDLE, LPOVERLAPPED, LPDWORD, BOOL);
using GetOverlappedResultExFn = BOOL(WINAPI*)(HANDLE, LPOVERLAPPED, LPDWORD, DWORD, BOOL);
using HidGetInputReportFn = BOOLEAN(__stdcall*)(HANDLE, PVOID, ULONG);
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
// hidsdi.h's HIDD_ATTRIBUTES and HidD_GetAttributes, without the driver kit's headers.
struct HidAttributes {
    ULONG size;
    USHORT vendor;
    USHORT product;
    USHORT version;
};
using HidGetAttributesFn = BOOLEAN(__stdcall*)(HANDLE, HidAttributes*);

CreateFileWFn g_createFileW = nullptr;
CloseHandleFn g_closeHandle = nullptr;
ReadFileFn g_readFile = nullptr;
GetOverlappedResultFn g_getOverlappedResult = nullptr;
GetOverlappedResultExFn g_getOverlappedResultEx = nullptr;
HidGetInputReportFn g_hidGetInputReport = nullptr;
HidGetAttributesFn g_hidGetAttributes = nullptr;  // called, not hooked

// The handles the game opened on Sony controllers. Every file goes through these hooks, so
// the common case, no controller open, is one read of a counter.
constexpr size_t kMaxPads = 8;
struct OpenPad {
    HANDLE handle;
    SonyPad pad;
};
SRWLOCK g_padsLock = SRWLOCK_INIT;
OpenPad g_pads[kMaxPads] = {};
volatile LONG g_padCount = 0;

// Reads from them still in flight, finished by GetOverlappedResult.
constexpr size_t kMaxReads = 16;
struct PendingRead {
    OVERLAPPED* overlapped;
    void* buffer;
    SonyPad pad;
};
SRWLOCK g_readsLock = SRWLOCK_INIT;
PendingRead g_reads[kMaxReads] = {};
size_t g_nextRead = 0;

// Handles already asked whether they're a Sony controller's: the game may have opened its
// controller before the hook came, so reads are looked at too, each handle once.
constexpr size_t kMaxChecked = 64;
SRWLOCK g_checkedLock = SRWLOCK_INIT;
HANDLE g_checked[kMaxChecked] = {};
size_t g_nextChecked = 0;
// Reads of these sizes may be a controller's report (64 bytes on USB); others aren't asked about.
constexpr DWORD kMinReportRead = 64;
constexpr DWORD kMaxReportRead = 1024;

SonyPad PadFor(HANDLE handle) {
    if (g_padCount == 0 || handle == INVALID_HANDLE_VALUE) {
        return SonyPad::None;
    }
    SonyPad pad = SonyPad::None;
    AcquireSRWLockShared(&g_padsLock);
    for (const OpenPad& open : g_pads) {
        if (open.handle == handle) {
            pad = open.pad;
        }
    }
    ReleaseSRWLockShared(&g_padsLock);
    return pad;
}

void Remember(HANDLE handle, SonyPad pad) {
    AcquireSRWLockExclusive(&g_padsLock);
    for (OpenPad& open : g_pads) {
        if (!open.handle) {
            open = {handle, pad};
            InterlockedIncrement(&g_padCount);
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_padsLock);
}

// Whether `handle` was asked about already; notes that it has been, if not.
bool CheckedBefore(HANDLE handle) {
    AcquireSRWLockExclusive(&g_checkedLock);
    bool checked = false;
    for (HANDLE seen : g_checked) {
        checked |= seen == handle;
    }
    if (!checked) {
        g_checked[g_nextChecked] = handle;
        g_nextChecked = (g_nextChecked + 1) % kMaxChecked;
    }
    ReleaseSRWLockExclusive(&g_checkedLock);
    return checked;
}

void Uncheck(HANDLE handle) {
    AcquireSRWLockExclusive(&g_checkedLock);
    for (HANDLE& seen : g_checked) {
        if (seen == handle) {
            seen = nullptr;
        }
    }
    ReleaseSRWLockExclusive(&g_checkedLock);
}

// The pad a handle the game reads `size` bytes from is, asking HID once per handle.
SonyPad PadReadFrom(HANDLE handle, DWORD size) {
    SonyPad pad = PadFor(handle);
    if (pad != SonyPad::None || !g_hidGetAttributes || size < kMinReportRead ||
        size > kMaxReportRead || CheckedBefore(handle)) {
        return pad;
    }
    const DWORD error = GetLastError();
    HidAttributes attributes = {sizeof(attributes)};
    if (g_hidGetAttributes(handle, &attributes)) {
        pad = SonyPadWithIds(attributes.vendor, attributes.product);
    }
    if (pad != SonyPad::None) {
        Remember(handle, pad);
    }
    SetLastError(error);
    return pad;
}

void Forget(HANDLE handle) {
    Uncheck(handle);
    if (g_padCount == 0) {
        return;
    }
    AcquireSRWLockExclusive(&g_padsLock);
    for (OpenPad& open : g_pads) {
        if (open.handle == handle) {
            open = {};
            InterlockedDecrement(&g_padCount);
        }
    }
    ReleaseSRWLockExclusive(&g_padsLock);
}

void Track(OVERLAPPED* overlapped, void* buffer, SonyPad pad) {
    AcquireSRWLockExclusive(&g_readsLock);
    PendingRead* slot = &g_reads[g_nextRead];
    for (PendingRead& read : g_reads) {
        if (read.overlapped == overlapped) {
            slot = &read;  // the same OVERLAPPED again: its last read is done with
        }
    }
    if (slot == &g_reads[g_nextRead]) {
        g_nextRead = (g_nextRead + 1) % kMaxReads;
    }
    *slot = {overlapped, buffer, pad};
    ReleaseSRWLockExclusive(&g_readsLock);
}

// The read `overlapped` started, once and for all; empty if it isn't one of the pads'.
PendingRead Finish(OVERLAPPED* overlapped) {
    PendingRead found = {};
    AcquireSRWLockExclusive(&g_readsLock);
    for (PendingRead& read : g_reads) {
        if (read.overlapped == overlapped) {
            found = read;
            read = {};
        }
    }
    ReleaseSRWLockExclusive(&g_readsLock);
    return found;
}

void Patch(SonyPad pad, void* buffer, DWORD size, LONG path) {
    SonyUsbReport report(pad, static_cast<uint8_t*>(buffer), size);
    if (report.Valid()) {
        report.AddToRightStick(HeadTurnForRead(report.Active(), path));
    }
}

void PatchFinished(OVERLAPPED* overlapped, DWORD size) {
    PendingRead read = Finish(overlapped);
    if (read.overlapped) {
        Patch(read.pad, read.buffer, size, kPathSonyOverlapped);
    }
}

// A cheap look before the full one: Sony's vendor id in the path.
bool MaybeSony(LPCWSTR name) { return name && (wcsstr(name, L"054c") || wcsstr(name, L"054C")); }

HANDLE WINAPI CreateFileWDetour(LPCWSTR name, DWORD access, DWORD share,
                                LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags,
                                HANDLE templateFile) {
    HANDLE handle = g_createFileW(name, access, share, security, disposition, flags, templateFile);
    if (handle != INVALID_HANDLE_VALUE && MaybeSony(name)) {
        const DWORD error = GetLastError();
        const SonyPad pad = SonyPadAt(name);
        if (pad != SonyPad::None) {
            Remember(handle, pad);
        }
        SetLastError(error);
    }
    return handle;
}

BOOL WINAPI CloseHandleDetour(HANDLE handle) {
    Forget(handle);
    return g_closeHandle(handle);
}

BOOL WINAPI ReadFileDetour(HANDLE handle, LPVOID buffer, DWORD size, LPDWORD read,
                           LPOVERLAPPED overlapped) {
    const SonyPad pad = PadReadFrom(handle, size);
    BOOL ok = g_readFile(handle, buffer, size, read, overlapped);
    if (pad == SonyPad::None) {
        return ok;
    }
    const DWORD error = GetLastError();
    if (ok) {
        DWORD got = read ? *read : overlapped ? DWORD(overlapped->InternalHigh) : 0;
        Patch(pad, buffer, got, kPathSonyRead);
    } else if (overlapped && error == ERROR_IO_PENDING) {
        Track(overlapped, buffer, pad);
    }
    SetLastError(error);
    return ok;
}

BOOL WINAPI GetOverlappedResultDetour(HANDLE handle, LPOVERLAPPED overlapped, LPDWORD size,
                                      BOOL wait) {
    BOOL ok = g_getOverlappedResult(handle, overlapped, size, wait);
    if (ok && g_padCount != 0 && size) {
        PatchFinished(overlapped, *size);
    }
    return ok;
}

BOOL WINAPI GetOverlappedResultExDetour(HANDLE handle, LPOVERLAPPED overlapped, LPDWORD size,
                                        DWORD milliseconds, BOOL alertable) {
    BOOL ok = g_getOverlappedResultEx(handle, overlapped, size, milliseconds, alertable);
    if (ok && g_padCount != 0 && size) {
        PatchFinished(overlapped, *size);
    }
    return ok;
}

BOOLEAN __stdcall HidGetInputReportDetour(HANDLE handle, PVOID buffer, ULONG size) {
    BOOLEAN ok = g_hidGetInputReport(handle, buffer, size);
    const SonyPad pad = ok ? PadReadFrom(handle, size) : SonyPad::None;
    if (pad != SonyPad::None) {
        Patch(pad, buffer, size, kPathSonyGetReport);
    }
    return ok;
}

// XInput: each of its DLLs has its own XInputGetState, so one detour per DLL.
const wchar_t* const kXInputDlls[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
constexpr size_t kXInputCount = sizeof(kXInputDlls) / sizeof(kXInputDlls[0]);
XInputGetStateFn g_xinputGetState[kXInputCount] = {};

template <size_t N>
DWORD WINAPI XInputGetStateDetour(DWORD user, XINPUT_STATE* state) {
    DWORD result = g_xinputGetState[N](user, state);
    if (result == ERROR_SUCCESS && state) {
        XInputPadState pad(state->Gamepad);
        pad.AddToRightStick(HeadTurnForRead(pad.Active(), kPathXInput));
    }
    return result;
}

const XInputGetStateFn kXInputDetours[kXInputCount] = {
    XInputGetStateDetour<0>, XInputGetStateDetour<1>, XInputGetStateDetour<2>};

template <typename Fn>
bool HookExport(HMODULE module, const char* name, Fn detour, Fn* original) {
    void* target = module ? ModuleExport(module, name) : nullptr;
    return target && CreateHook(name, target, detour, original);
}

bool InstallFiles() {
    HMODULE kernel = GetModuleHandleW(L"kernelbase.dll");
    return HookExport(kernel, "CreateFileW", &CreateFileWDetour, &g_createFileW) &&
           HookExport(kernel, "CloseHandle", &CloseHandleDetour, &g_closeHandle) &&
           HookExport(kernel, "ReadFile", &ReadFileDetour, &g_readFile) &&
           HookExport(kernel, "GetOverlappedResult", &GetOverlappedResultDetour,
                      &g_getOverlappedResult) &&
           HookExport(kernel, "GetOverlappedResultEx", &GetOverlappedResultExDetour,
                      &g_getOverlappedResultEx);
}

void InstallHid() {
    // A system DLL: loading it early, before the game does, changes nothing for it.
    HMODULE hid = LoadLibraryW(L"hid.dll");
    if (hid) {
        g_hidGetAttributes =
            reinterpret_cast<HidGetAttributesFn>(ModuleExport(hid, "HidD_GetAttributes"));
    }
    HookExport(hid, "HidD_GetInputReport", &HidGetInputReportDetour, &g_hidGetInputReport);
}

void InstallXInput() {
    for (size_t i = 0; i < kXInputCount; ++i) {
        // xinput1_3 comes with the game's DirectX runtime, if at all: only once it's loaded.
        HMODULE module = i == 1 ? GetModuleHandleW(kXInputDlls[i]) : LoadLibraryW(kXInputDlls[i]);
        HookExport(module, "XInputGetState", kXInputDetours[i], &g_xinputGetState[i]);
    }
}

}  // namespace

bool Install() {
    if (!InstallFiles()) {
        SetStatus("controller hooks failed: no head turns through the controller");
        return false;
    }
    InstallHid();
    InstallXInput();
    return true;
}

}  // namespace gazeora::hook::input
