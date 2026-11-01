// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <string>

namespace frame_recorder {

// Repository pattern: the plugin's settings as key=value lines in frame-recorder.ini, in the
// folder Gazeora keeps its own settings in.
class SettingsFile {
public:
    using Values = std::map<std::string, std::string>;

    // `folder`: Gazeora's settings folder (UTF-8). "" keeps nothing, as Gazeora's tests ask.
    explicit SettingsFile(std::string folder);

    Values Load() const;
    void Save(const Values& values) const;

private:
    std::string m_folder;
};

}  // namespace frame_recorder
