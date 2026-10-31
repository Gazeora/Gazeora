// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Where library.json lives.
class LibraryLocation final {
public:
    LibraryLocation() = delete;

    // %APPDATA%\Gazeora\library.json, or %GAZEORA_LIBRARY% if set.
    static std::wstring DefaultPath();
};

}  // namespace gazeora
