// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings_file.h"

#include <fstream>
#include <utility>

#include "utf8_path.h"

namespace frame_recorder {

namespace {

// Named after the plugin, beside Gazeora's own settings files.
constexpr char kFileName[] = "frame-recorder.ini";
constexpr char kSeparator = '=';

}  // namespace

SettingsFile::SettingsFile(std::string folder) : m_folder(std::move(folder)) {}

SettingsFile::Values SettingsFile::Load() const {
    Values values;
    if (m_folder.empty()) {
        return values;
    }
    std::ifstream file(Utf8Path::From(m_folder) / kFileName);
    std::string line;
    while (std::getline(file, line)) {
        const size_t separator = line.find(kSeparator);
        if (separator != std::string::npos) {
            values[line.substr(0, separator)] = line.substr(separator + 1);
        }
    }
    return values;
}

void SettingsFile::Save(const Values& values) const {
    if (m_folder.empty()) {
        return;
    }
    std::ofstream file(Utf8Path::From(m_folder) / kFileName, std::ios::trunc);
    for (const auto& [key, value] : values) {
        file << key << kSeparator << value << '\n';
    }
}

}  // namespace frame_recorder
