#pragma once

#include <string>

#include "gazeora/input/models/hand_gesture_settings.h"

namespace gazeora::app {

// Repository pattern: the controls settings, in controls.ini next to the library file so
// they're easy to edit by hand. Hand gestures in [hand_gestures].
class ControlsSettings {
public:
    // The default location: next to LibraryLocation::DefaultPath().
    ControlsSettings();
    explicit ControlsSettings(std::wstring path) : m_path(std::move(path)) {}

    // Off, with the first camera, unless saved otherwise.
    input::HandGestureSettings LoadHandGestures() const;
    void SaveHandGestures(const input::HandGestureSettings& settings) const;

    const std::wstring& Path() const { return m_path; }

private:
    std::wstring ReadText(const wchar_t* key) const;
    void WriteText(const wchar_t* key, const std::wstring& value) const;

    std::wstring m_path;
};

}  // namespace gazeora::app
