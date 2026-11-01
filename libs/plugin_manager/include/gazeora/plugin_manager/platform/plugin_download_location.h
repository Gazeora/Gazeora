// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Where downloaded plugins wait to be installed: %TEMP%\Gazeora\plugins.
class PluginDownloadLocation final {
public:
    PluginDownloadLocation() = delete;
    static std::wstring Folder();
};

}  // namespace gazeora
