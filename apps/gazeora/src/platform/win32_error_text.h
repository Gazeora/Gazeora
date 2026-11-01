// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <cwchar>
#include <string>

namespace gazeora::app {

// "<what>: <Windows' message for code>", for errors shown to the user.
inline std::wstring Win32ErrorText(const wchar_t* what, DWORD code = GetLastError()) {
    wchar_t* text = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring message = std::wstring(what) + L": ";
    if (text) {
        message += text;
        LocalFree(text);
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
            message.pop_back();
        }
    } else {
        wchar_t hex[16];
        swprintf(hex, 16, L"0x%08lX", static_cast<unsigned long>(code));
        message += hex;
    }
    return message;
}

}  // namespace gazeora::app
