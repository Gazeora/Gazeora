// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include "gazeora/common/text/utf8.h"

namespace gazeora {

// A buffer a plugin writes UTF-8 text into, read back safely even if the plugin forgot the
// NUL or wrote nothing; and the other way, text written into a buffer a caller passes.
class PluginText final {
public:
    static constexpr size_t kCapacity = 1024;

    PluginText() { m_buffer[0] = '\0'; }

    char* Data() { return m_buffer.data(); }
    size_t Capacity() const { return m_buffer.size(); }
    // What the plugin wrote, as it wrote it (UTF-8).
    std::string Utf8Text() const {
        const void* end = std::memchr(m_buffer.data(), '\0', m_buffer.size());
        size_t length = end ? static_cast<const char*>(end) - m_buffer.data() : m_buffer.size();
        return std::string(m_buffer.data(), length);
    }
    std::wstring Wide() const { return Utf8::ToWide(Utf8Text()); }

    // A string a plugin hands over; empty for null.
    static std::string Narrow(const char* text) { return text ? std::string(text) : std::string(); }
    static std::wstring Wide(const char* text) { return Utf8::ToWide(Narrow(text)); }
    // Writes `text` into a buffer Gazeora or a plugin passes, truncated to fit with its NUL.
    static void Write(const std::string& text, char* buffer, size_t capacity) {
        if (!buffer || capacity == 0) {
            return;
        }
        const size_t length = std::min(text.size(), capacity - 1);
        std::memcpy(buffer, text.data(), length);
        buffer[length] = '\0';
    }

private:
    std::array<char, kCapacity> m_buffer;
};

}  // namespace gazeora
