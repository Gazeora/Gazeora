// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings/settings_file.h"

#include "gazeora/library/platform/library_location.h"

namespace gazeora::app {

std::wstring SettingsFile::NextToLibrary(const wchar_t* name) {
    std::wstring library = LibraryLocation::DefaultPath();
    return library.substr(0, library.find_last_of(L"\\/") + 1) + name;
}

}  // namespace gazeora::app
