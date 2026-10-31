// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>
#include <winhttp.h>

namespace gazeora {

// Owns one WinHTTP handle (session, connection or request) and closes it.
class WinHttpHandle final {
public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : m_handle(handle) {}
    ~WinHttpHandle() { Close(); }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle(WinHttpHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HINTERNET Get() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }

private:
    void Close() {
        if (m_handle) {
            WinHttpCloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

    HINTERNET m_handle = nullptr;
};

}  // namespace gazeora
