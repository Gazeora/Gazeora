// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recording.h"

#include <system_error>

#include "file_start.h"
#include "recording_name.h"
#include "utf8_path.h"

namespace frame_recorder {

namespace {

// Tone ids a card's text can take (docs/plugin-ui.md#tones).
constexpr char kDim[] = "dim";
constexpr char kSuccess[] = "success";
constexpr char kDanger[] = "danger";
// The extension of a recording whose frames don't name their codec.
constexpr char kUnknownCodec[] = "annexb";

}  // namespace

Recording::Recording(const RecorderSettings& settings) : m_settings(settings) {}

void Recording::Begin() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active = true;
    m_failed = false;
    m_keyFrameWanted = true;
}

void Recording::End() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active = false;
    CloseLocked();
}

void Recording::Restart() {
    std::lock_guard<std::mutex> lock(m_mutex);
    CloseLocked();
    m_failed = false;
    m_keyFrameWanted = true;
}

bool Recording::TakeKeyFrameRequest() { return m_keyFrameWanted.exchange(false); }

Recording::WriteResult Recording::Write(const GazeoraFrameV2& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active || m_failed || !m_settings.Recording()) {
        return WriteResult::Skipped;
    }
    WriteResult result = WriteResult::Written;
    GazeoraBytesV2 bytes = {frame.annexb, frame.size};
    if (!m_file.IsOpen()) {
        if (frame.key_frame != GAZEORA_TRUE) {
            return WriteResult::Skipped;
        }
        if (!OpenLocked(frame.codec_id ? frame.codec_id : kUnknownCodec)) {
            return WriteResult::Failed;
        }
        result = WriteResult::Opened;
        bytes = FileStart::Of(frame);
    }
    if (!m_file.Write(bytes.data, bytes.size)) {
        CloseLocked();
        FailLocked();  // a full disk, say
        return WriteResult::Failed;
    }
    ++m_frames;
    return result;
}

RecordingStatus Recording::Status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_settings.Recording()) {
        return {"Off.", kDim};
    }
    if (!m_active) {
        return {"Records while Record to file is the stream's protocol.", kDim};
    }
    if (m_failed) {
        return {"Can't write to " + m_failedFolder + ".", kDanger};
    }
    if (m_file.IsOpen()) {
        return {"Recording to " + m_fileName + ": " + std::to_string(m_frames) + " frames.",
                kSuccess};
    }
    return {"Waiting for a game's frames.", kDim};
}

std::string Recording::FileName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fileName;
}

bool Recording::OpenLocked(const std::string& codecId) {
    const std::filesystem::path folder = Utf8Path::From(m_settings.Folder());
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    const std::filesystem::path path = RecordingName::In(folder, codecId);
    if (folder.empty() || !m_file.Open(path)) {
        FailLocked();
        return false;
    }
    m_fileName = Utf8Path::Of(path);
    m_frames = 0;
    return true;
}

void Recording::FailLocked() {
    // Until the player turns recording off and on, or starts a new file.
    m_failed = true;
    m_failedFolder = m_settings.Folder();
}

void Recording::CloseLocked() {
    m_file.Close();
    m_fileName.clear();
}

}  // namespace frame_recorder
