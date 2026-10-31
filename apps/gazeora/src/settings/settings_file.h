// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::app {

// Where the launcher keeps its files: next to the library file, in %APPDATA%\Gazeora.
class SettingsFile {
public:
    static std::wstring NextToLibrary(const wchar_t* name);
};

}  // namespace gazeora::app
