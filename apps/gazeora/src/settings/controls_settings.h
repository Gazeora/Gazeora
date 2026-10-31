// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::app {

// Repository pattern: the controls settings, in controls.ini next to the library file so
// they're easy to edit by hand. Game controllers in [game_controller], head tracking in
// [head_tracking].
class ControlsSettings {
public:
    // The default location: next to LibraryLocation::DefaultPath().
    ControlsSettings();
    explicit ControlsSettings(std::wstring path) : m_path(std::move(path)) {}

    // On unless saved off.
    bool LoadGamepadEnabled() const;
    void SaveGamepadEnabled(bool enabled) const;

    // On unless saved off.
    bool LoadHeadTrackingEnabled() const;
    void SaveHeadTrackingEnabled(bool enabled) const;
    // Mouse counts per degree the head turns; `fallback` if none is saved.
    float LoadHeadCountsPerDegree(float fallback) const;
    void SaveHeadCountsPerDegree(float counts) const;
    // How the game turns with its controller's stick: degrees a second at full tilt, and the
    // dead zone (0..1) it ignores; `fallback` if none is saved.
    float LoadStickDegreesPerSecond(float fallback) const;
    void SaveStickDegreesPerSecond(float degrees) const;
    float LoadStickDeadZone(float fallback) const;
    void SaveStickDeadZone(float deadZone) const;

    const std::wstring& Path() const { return m_path; }

private:
    // A number, 0 or more, under `key` in [head_tracking]; `fallback` if there's none.
    float LoadHeadNumber(const wchar_t* key, float fallback) const;
    void SaveHeadNumber(const wchar_t* key, float value) const;
    bool LoadSwitch(const wchar_t* section) const;
    void SaveSwitch(const wchar_t* section, bool enabled) const;
    std::wstring ReadText(const wchar_t* section, const wchar_t* key) const;
    void WriteText(const wchar_t* section, const wchar_t* key, const std::wstring& value) const;
    void CreateFolder() const;

    std::wstring m_path;
};

}  // namespace gazeora::app
