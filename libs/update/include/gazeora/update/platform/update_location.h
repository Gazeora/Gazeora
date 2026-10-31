// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Where downloaded updates wait to be installed: %TEMP%\Gazeora\updates.
class UpdateLocation final {
public:
    UpdateLocation() = delete;

    static std::wstring DownloadFolder();
};

}  // namespace gazeora
