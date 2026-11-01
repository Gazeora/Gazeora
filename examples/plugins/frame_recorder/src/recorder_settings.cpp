// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recorder_settings.h"

#include <utility>

namespace frame_recorder {

namespace {

constexpr char kRecordingKey[] = "recording";
constexpr char kFolderKey[] = "folder";
constexpr char kOn[] = "1";
constexpr char kOff[] = "0";

}  // namespace

RecorderSettings::RecorderSettings(SettingsFile file, std::string defaultFolder)
    : m_file(std::move(file)), m_folder(std::move(defaultFolder)) {
    const SettingsFile::Values values = m_file.Load();
    if (auto recording = values.find(kRecordingKey); recording != values.end()) {
        m_recording = recording->second == kOn;
    }
    if (auto folder = values.find(kFolderKey); folder != values.end() && !folder->second.empty()) {
        m_folder = folder->second;
    }
}

bool RecorderSettings::Recording() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_recording;
}

void RecorderSettings::SetRecording(bool recording) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recording = recording;
    SaveLocked();
}

std::string RecorderSettings::Folder() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_folder;
}

void RecorderSettings::SetFolder(std::string folder) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_folder = std::move(folder);
    SaveLocked();
}

void RecorderSettings::SaveLocked() const {
    m_file.Save({{kRecordingKey, m_recording ? kOn : kOff}, {kFolderKey, m_folder}});
}

}  // namespace frame_recorder
