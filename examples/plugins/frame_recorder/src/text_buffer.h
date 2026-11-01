// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

namespace frame_recorder {

// Text a plugin hands back goes into Gazeora's buffer, cut to its capacity (which counts the NUL).
class TextBuffer {
public:
    static void Write(const std::string& text, char* buffer, size_t capacity) {
        if (buffer && capacity > 0) {
            std::snprintf(buffer, capacity, "%s", text.c_str());
        }
    }
};

}  // namespace frame_recorder
