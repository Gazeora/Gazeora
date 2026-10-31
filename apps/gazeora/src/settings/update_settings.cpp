// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings/update_settings.h"

#include <windows.h>
#include <shlobj.h>

#include "platform/shell.h"
#include "settings/settings_file.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kSection[] = L"updates";
constexpr wchar_t kCheckOnStart[] = L"check_on_start";
constexpr wchar_t kOn[] = L"1";
constexpr wchar_t kOff[] = L"0";
constexpr INT kDefaultOn = 1;

}  // namespace

UpdateSettings::UpdateSettings() : m_path(SettingsFile::NextToLibrary(L"updates.ini")) {}

bool UpdateSettings::LoadCheckOnStart() const {
    return GetPrivateProfileIntW(kSection, kCheckOnStart, kDefaultOn, m_path.c_str()) != 0;
}

void UpdateSettings::SaveCheckOnStart(bool check) const {
    SHCreateDirectoryExW(nullptr, FolderOf(m_path).c_str(), nullptr);
    WritePrivateProfileStringW(kSection, kCheckOnStart, check ? kOn : kOff, m_path.c_str());
}

}  // namespace gazeora::app
