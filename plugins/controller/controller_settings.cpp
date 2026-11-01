// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "controller_settings.h"

#include <windows.h>
#include <shlobj.h>

namespace gazeora {

namespace {

constexpr wchar_t kFile[] = L"\\controls.ini";
constexpr wchar_t kSection[] = L"game_controller";
constexpr wchar_t kEnabled[] = L"enabled";
constexpr wchar_t kOn[] = L"1";
constexpr wchar_t kOff[] = L"0";
constexpr DWORD kMaxText = 16;

}  // namespace

ControllerSettings::ControllerSettings(const std::wstring& folder)
    : m_path(folder.empty() ? std::wstring() : folder + kFile), m_enabled(Load()) {}

void ControllerSettings::SetEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_path.empty()) {
        return;
    }
    SHCreateDirectoryExW(nullptr, m_path.substr(0, m_path.find_last_of(L'\\')).c_str(), nullptr);
    WritePrivateProfileStringW(kSection, kEnabled, enabled ? kOn : kOff, m_path.c_str());
}

bool ControllerSettings::Load() const {
    if (m_path.empty()) {
        return true;
    }
    wchar_t text[kMaxText];
    GetPrivateProfileStringW(kSection, kEnabled, L"", text, kMaxText, m_path.c_str());
    return std::wstring(text) != kOff;
}

}  // namespace gazeora
