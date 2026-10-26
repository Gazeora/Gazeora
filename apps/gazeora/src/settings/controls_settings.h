#pragma once

#include <string>

namespace gazeora::app {

// Repository pattern: the controls settings, in controls.ini next to the library file so
// they're easy to edit by hand. Game controllers in [game_controller].
class ControlsSettings {
public:
    // The default location: next to LibraryLocation::DefaultPath().
    ControlsSettings();
    explicit ControlsSettings(std::wstring path) : m_path(std::move(path)) {}

    // On unless saved off.
    bool LoadGamepadEnabled() const;
    void SaveGamepadEnabled(bool enabled) const;

    const std::wstring& Path() const { return m_path; }

private:
    std::wstring ReadText(const wchar_t* section, const wchar_t* key) const;
    void WriteText(const wchar_t* section, const wchar_t* key, const std::wstring& value) const;
    void CreateFolder() const;

    std::wstring m_path;
};

}  // namespace gazeora::app
