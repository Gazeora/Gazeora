// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/platform/update_location.h"

#include <windows.h>

namespace gazeora {

namespace {

constexpr wchar_t kSubfolder[] = L"Gazeora\\updates";

}  // namespace

std::wstring UpdateLocation::DownloadFolder() {
    wchar_t temp[MAX_PATH + 1];
    const DWORD length = GetTempPathW(MAX_PATH + 1, temp);  // ends in a backslash
    return std::wstring(temp, length) + kSubfolder;
}

}  // namespace gazeora
