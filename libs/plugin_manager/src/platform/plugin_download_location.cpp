// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/platform/plugin_download_location.h"

#include <windows.h>

namespace gazeora {

namespace {

constexpr wchar_t kSubfolder[] = L"Gazeora\\plugins";

}  // namespace

std::wstring PluginDownloadLocation::Folder() {
    wchar_t temp[MAX_PATH + 1] = {};
    DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    return std::wstring(temp, length) + kSubfolder;
}

}  // namespace gazeora
