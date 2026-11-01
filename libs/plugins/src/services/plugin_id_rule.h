// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstring>
#include <initializer_list>

namespace gazeora {

// What a plugin's ids (stores, codecs, protocols) may be: lowercase a-z, 0-9 and '-', at most
// 64 characters, and none of the ids reserved for that kind.
class PluginIdRule final {
public:
    PluginIdRule() = delete;

    static bool Accepts(const char* id, std::initializer_list<const char*> reserved) {
        if (!id || !*id || std::strlen(id) > kMaxLength) {
            return false;
        }
        for (const char* taken : reserved) {
            if (std::strcmp(id, taken) == 0) {
                return false;
            }
        }
        for (const char* c = id; *c; ++c) {
            if (!IsIdCharacter(*c)) {
                return false;
            }
        }
        return true;
    }

private:
    static constexpr size_t kMaxLength = 64;

    static bool IsIdCharacter(char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    }
};

}  // namespace gazeora
