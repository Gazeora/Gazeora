#include "settings/controls_settings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwchar>

#include "platform/shell.h"
#include "settings/settings_file.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kHandSection[] = L"hand_gestures";
constexpr wchar_t kCamera[] = L"camera";
constexpr wchar_t kSpeed[] = L"pointer_speed";
constexpr wchar_t kGamepadSection[] = L"game_controller";
constexpr wchar_t kEnabled[] = L"enabled";
constexpr wchar_t kOn[] = L"1";
constexpr wchar_t kOff[] = L"0";
constexpr DWORD kMaxText = 1024;  // camera ids are device paths
constexpr size_t kSpeedText = 32;

}  // namespace

ControlsSettings::ControlsSettings() : m_path(SettingsFile::NextToLibrary(L"controls.ini")) {}

input::HandGestureSettings ControlsSettings::LoadHandGestures() const {
    input::HandGestureSettings settings;
    settings.cameraId = ReadText(kHandSection, kCamera);
    std::wstring speed = ReadText(kHandSection, kSpeed);
    if (!speed.empty()) {
        settings.speed =
            std::clamp(float(_wtof(speed.c_str())), input::HandGestureSettings::kMinSpeed,
                       input::HandGestureSettings::kMaxSpeed);
    }
    return settings;
}

void ControlsSettings::SaveHandGestures(const input::HandGestureSettings& settings) const {
    CreateFolder();
    wchar_t speed[kSpeedText];
    swprintf(speed, kSpeedText, L"%.1f", settings.speed);
    WriteText(kHandSection, kCamera, settings.cameraId);
    WriteText(kHandSection, kSpeed, speed);
}

bool ControlsSettings::LoadGamepadEnabled() const {
    return ReadText(kGamepadSection, kEnabled) != kOff;
}

void ControlsSettings::SaveGamepadEnabled(bool enabled) const {
    CreateFolder();
    WriteText(kGamepadSection, kEnabled, enabled ? kOn : kOff);
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
