// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/stream_log.h"

#include <windows.h>

#include <algorithm>
#include <cstdarg>

namespace gazeora {

namespace {
constexpr long kMaxLogBytes = 4 << 20;
constexpr size_t kMaxLineChars = 512;
}  // namespace

void StreamLog::Open(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_bytes = 0;
    if (!path.empty()) {
        m_file = _wfopen(path.c_str(), L"w");
    }
}

bool StreamLog::Writing() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_file && m_bytes <= kMaxLogBytes;
}

void StreamLog::Write(const char* format, ...) {
    if (!m_forward && !Writing()) {
        return;  // don't format what nobody will read
    }
    char text[kMaxLineChars];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (m_forward) {
        m_forward(text);
    } else {
        Append(text);
    }
}

void StreamLog::Append(const char* text) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file) {
        return;
    }
    int written = fprintf(m_file, "%02d:%02d:%02d.%03d  %s\n", time.wHour, time.wMinute,
                          time.wSecond, time.wMilliseconds, text);
    fflush(m_file);
    m_bytes += std::max(written, 0);
}

}  // namespace gazeora
