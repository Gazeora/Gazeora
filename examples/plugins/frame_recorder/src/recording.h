// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "annexb_file.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "recorder_settings.h"

namespace frame_recorder {

// What the card shows about the recording: a line of text and its tone (docs/plugin-ui.md).
struct RecordingStatus {
    std::string text;
    std::string tone;
};

// The recording, shared by the output, on Gazeora's encode thread, and the card, on its UI
// thread. Each file starts at a key frame, so it plays from its first frame.
class Recording {
public:
    enum class WriteResult { Skipped, Written, Opened, Failed };

    explicit Recording(const RecorderSettings& settings);

    // The protocol was chosen (Begin), or another one was (End).
    void Begin();
    void End();
    // The player changed a setting or asked for a new file: a new file from the next key frame.
    void Restart();

    // True once after Begin or Restart, so Gazeora makes the next frame a key frame.
    bool TakeKeyFrameRequest();
    WriteResult Write(const GazeoraFrameV2& frame);
    RecordingStatus Status() const;
    std::string FileName() const;  // UTF-8; "" while no file is open

private:
    bool OpenLocked(const std::string& codecId);
    void CloseLocked();
    void FailLocked();

    const RecorderSettings& m_settings;
    mutable std::mutex m_mutex;
    AnnexBFile m_file;
    std::string m_fileName;
    uint64_t m_frames = 0;
    bool m_active = false;
    bool m_failed = false;
    std::string m_failedFolder;  // UTF-8
    std::atomic<bool> m_keyFrameWanted{false};
};

}  // namespace frame_recorder
