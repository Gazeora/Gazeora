// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/platform/msiexec_runner.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

#include "gazeora/update/exceptions/update_error.h"

namespace gazeora {

namespace {

constexpr wchar_t kMsiexec[] = L"msiexec.exe";
constexpr wchar_t kInstallSwitch[] = L"/i ";

std::wstring SystemProgram(const wchar_t* name) {
    wchar_t folder[MAX_PATH];
    const UINT length = GetSystemDirectoryW(folder, MAX_PATH);
    return std::wstring(folder, length) + L"\\" + name;
}

}  // namespace

void MsiexecRunner::Run(const std::wstring& installerPath) {
    const std::wstring program = SystemProgram(kMsiexec);
    const std::wstring arguments = kInstallSwitch + (L"\"" + installerPath + L"\"");
    SHELLEXECUTEINFOW info = {sizeof(info)};
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = L"open";
    info.lpFile = program.c_str();
    info.lpParameters = arguments.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        throw UpdateError("Windows Installer couldn't be started");
    }
}

}  // namespace gazeora
