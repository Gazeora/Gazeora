// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings/controls_settings.h"

#include <windows.h>
#include <shlobj.h>

#include <cwchar>
#include <string>

#include "platform/shell.h"
#include "settings/settings_file.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kGamepadSection[] = L"game_controller";
constexpr wchar_t kHeadSection[] = L"head_tracking";
constexpr wchar_t kEnabled[] = L"enabled";
constexpr wchar_t kCountsPerDegree[] = L"counts_per_degree";
constexpr wchar_t kStickDegreesPerSecond[] = L"stick_degrees_per_second";
constexpr wchar_t kStickDeadZone[] = L"stick_dead_zone";
constexpr wchar_t kOn[] = L"1";
constexpr wchar_t kOff[] = L"0";
constexpr DWORD kMaxText = 1024;

}  // namespace

ControlsSettings::ControlsSettings() : m_path(SettingsFile::NextToLibrary(L"controls.ini")) {}

bool ControlsSettings::LoadGamepadEnabled() const { return LoadSwitch(kGamepadSection); }

void ControlsSettings::SaveGamepadEnabled(bool enabled) const {
    SaveSwitch(kGamepadSection, enabled);
}

bool ControlsSettings::LoadHeadTrackingEnabled() const { return LoadSwitch(kHeadSection); }

void ControlsSettings::SaveHeadTrackingEnabled(bool enabled) const {
    SaveSwitch(kHeadSection, enabled);
}

float ControlsSettings::LoadHeadCountsPerDegree(float fallback) const {
    return LoadHeadNumber(kCountsPerDegree, fallback);
}

void ControlsSettings::SaveHeadCountsPerDegree(float counts) const {
    SaveHeadNumber(kCountsPerDegree, counts);
}

float ControlsSettings::LoadStickDegreesPerSecond(float fallback) const {
    return LoadHeadNumber(kStickDegreesPerSecond, fallback);
}

void ControlsSettings::SaveStickDegreesPerSecond(float degrees) const {
    SaveHeadNumber(kStickDegreesPerSecond, degrees);
}

float ControlsSettings::LoadStickDeadZone(float fallback) const {
    return LoadHeadNumber(kStickDeadZone, fallback);
}

void ControlsSettings::SaveStickDeadZone(float deadZone) const {
    SaveHeadNumber(kStickDeadZone, deadZone);
}

float ControlsSettings::LoadHeadNumber(const wchar_t* key, float fallback) const {
    const std::wstring text = ReadText(kHeadSection, key);
    wchar_t* end = nullptr;
    const float number = std::wcstof(text.c_str(), &end);
    return text.empty() || *end != L'\0' || !(number >= 0.0f) ? fallback : number;
}

void ControlsSettings::SaveHeadNumber(const wchar_t* key, float value) const {
    CreateFolder();
    WriteText(kHeadSection, key, std::to_wstring(value));
}

bool ControlsSettings::LoadSwitch(const wchar_t* section) const {
    return ReadText(section, kEnabled) != kOff;
}

void ControlsSettings::SaveSwitch(const wchar_t* section, bool enabled) const {
    CreateFolder();
    WriteText(section, kEnabled, enabled ? kOn : kOff);
}

std::wstring ControlsSettings::ReadText(const wchar_t* section, const wchar_t* key) const {
    wchar_t text[kMaxText];
    GetPrivateProfileStringW(section, key, L"", text, kMaxText, m_path.c_str());
    return text;
}

void ControlsSettings::WriteText(const wchar_t* section, const wchar_t* key,
                                 const std::wstring& value) const {
    WritePrivateProfileStringW(section, key, value.c_str(), m_path.c_str());
}

void ControlsSettings::CreateFolder() const {
    SHCreateDirectoryExW(nullptr, FolderOf(m_path).c_str(), nullptr);
}

}  // namespace gazeora::app
