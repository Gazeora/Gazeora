// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace gazeora {

// Copies a struct a plugin hands over, as far as the plugin says it goes (its struct_size):
// a plugin built for an older version has a shorter one. What it doesn't have stays zero.
class PluginStruct final {
public:
    PluginStruct() = delete;

    template <typename Struct>
    static Struct Copy(const void* source, uint32_t declaredSize) {
        Struct copy = {};
        std::memcpy(&copy, source, std::min<size_t>(declaredSize, sizeof(Struct)));
        return copy;
    }

    // Whether a struct of `declaredSize` bytes has all of `Struct`.
    template <typename Struct>
    static bool Covers(uint32_t declaredSize) {
        return declaredSize >= sizeof(Struct);
    }
};

}  // namespace gazeora
