// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <mutex>
#include <string>

#include "settings_file.h"

namespace frame_recorder {

// What the player chose on the card: whether to record, and into which folder. The card changes
// them on Gazeora's UI thread while the encode thread reads them, so they're locked.
class RecorderSettings {
public:
    // Loads them from `file`; `defaultFolder` until the player picks one.
    RecorderSettings(SettingsFile file, std::string defaultFolder);

    bool Recording() const;
    void SetRecording(bool recording);
    std::string Folder() const;  // UTF-8
    void SetFolder(std::string folder);

private:
    void SaveLocked() const;

    SettingsFile m_file;
    mutable std::mutex m_mutex;
    bool m_recording = true;
    std::string m_folder;
};

}  // namespace frame_recorder
