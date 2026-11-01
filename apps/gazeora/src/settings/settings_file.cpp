// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings/settings_file.h"

#include "gazeora/library/platform/library_location.h"

namespace gazeora::app {

std::wstring SettingsFile::NextToLibrary(const wchar_t* name) {
    const std::wstring folder = LibraryLocation::DefaultFolder();
    return folder.empty() ? name : folder + L"\\" + name;
}

}  // namespace gazeora::app
