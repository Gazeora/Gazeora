// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <utility>

namespace gazeora::app {

// Repository pattern: the update settings, in updates.ini next to the library file, under
// [updates].
class UpdateSettings {
public:
    // The default location: next to LibraryLocation::DefaultPath().
    UpdateSettings();
    explicit UpdateSettings(std::wstring path) : m_path(std::move(path)) {}

    // Whether to look for a newer Gazeora at every start; on unless saved off.
    bool LoadCheckOnStart() const;
    void SaveCheckOnStart(bool check) const;

private:
    std::wstring m_path;
};

}  // namespace gazeora::app
