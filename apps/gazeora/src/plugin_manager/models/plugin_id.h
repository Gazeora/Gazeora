// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// A plugin's id: the name of its DLL in the plugins folder, without ".dll", in lowercase.
class PluginId final {
public:
    PluginId() = delete;

    // a-z, 0-9, '-' and '_', at most 64 characters.
    static bool IsValid(const std::wstring& id);
    // The id of the plugin installed at `path`.
    static std::wstring OfPath(const std::wstring& path);
    // An id for a DLL picked from disk: its name in lowercase, other characters as '-'. Empty
    // if no letter or digit is left.
    static std::wstring ForPickedFile(const std::wstring& path);
    // A bare file name ending in .dll, with no folder in it.
    static bool IsSafeDllName(const std::wstring& fileName);
};

}  // namespace gazeora
