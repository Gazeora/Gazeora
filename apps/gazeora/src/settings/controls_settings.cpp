#include "settings/controls_settings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwchar>

#include "platform/shell.h"
#include "settings/settings_file.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kSection[] = L"hand_gestures";
constexpr wchar_t kCamera[] = L"camera";
constexpr wchar_t kSpeed[] = L"pointer_speed";
constexpr DWORD kMaxText = 1024;  // camera ids are device paths
constexpr size_t kSpeedText = 32;

}  // namespace

ControlsSettings::ControlsSettings() : m_path(SettingsFile::NextToLibrary(L"controls.ini")) {}

input::HandGestureSettings ControlsSettings::LoadHandGestures() const {
    input::HandGestureSettings settings;
    settings.cameraId = ReadText(kCamera);
    std::wstring speed = ReadText(kSpeed);
    if (!speed.empty()) {
        settings.speed =
            std::clamp(float(_wtof(speed.c_str())), input::HandGestureSettings::kMinSpeed,
                       input::HandGestureSettings::kMaxSpeed);
    }
    return settings;
}

void ControlsSettings::SaveHandGestures(const input::HandGestureSettings& settings) const {
    SHCreateDirectoryExW(nullptr, FolderOf(m_path).c_str(), nullptr);
    wchar_t speed[kSpeedText];
    swprintf(speed, kSpeedText, L"%.1f", settings.speed);
    WriteText(kCamera, settings.cameraId);
    WriteText(kSpeed, speed);
}

std::wstring ControlsSettings::ReadText(const wchar_t* key) const {
    wchar_t text[kMaxText];
    GetPrivateProfileStringW(kSection, key, L"", text, kMaxText, m_path.c_str());
    return text;
}

void ControlsSettings::WriteText(const wchar_t* key, const std::wstring& value) const {
    WritePrivateProfileStringW(kSection, key, value.c_str(), m_path.c_str());
}

}  // namespace gazeora::app
