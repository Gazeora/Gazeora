// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recorder_panel.h"

#include "guarded.h"
#include "recorder_panel_xml.h"  // kRecorderPanelXml, made from ui/recorder_panel.xml by the build
#include "text_buffer.h"

namespace frame_recorder {

namespace {

// The keys and the action ui/recorder_panel.xml binds to.
constexpr char kRecordingKey[] = "recording";
constexpr char kFolderKey[] = "folder";
constexpr char kStatusKey[] = "status";
constexpr char kToneKey[] = "tone";
constexpr char kFileOpenKey[] = "file-open";
constexpr char kNewFileAction[] = "new-file";

// A toggle's value, and a condition's (visible-if, enabled-if).
constexpr char kOn[] = "1";
constexpr char kOff[] = "0";

}  // namespace

RecorderPanel::RecorderPanel(RecorderSettings& settings, Recording& recording)
    : m_settings(settings),
      m_recording(recording),
      m_getters(Getters()),
      m_setters(Setters()),
      m_panel{sizeof(GazeoraUiPanelV3), this, kRecorderPanelXml, Get, Set, Action} {}

const GazeoraUiPanelV3& RecorderPanel::Struct() const { return m_panel; }

void GAZEORA_CALL RecorderPanel::Get(void* self, const char* key, char* value, size_t capacity) {
    Guarded::Run([&] {
        const auto& panel = *static_cast<const RecorderPanel*>(self);
        auto getter = panel.m_getters.find(key);
        TextBuffer::Write(getter != panel.m_getters.end() ? getter->second() : "", value, capacity);
    });
}

void GAZEORA_CALL RecorderPanel::Set(void* self, const char* key, const char* value) {
    Guarded::Run([&] {
        const auto& panel = *static_cast<const RecorderPanel*>(self);
        if (auto setter = panel.m_setters.find(key); setter != panel.m_setters.end()) {
            setter->second(value);
        }
    });
}

void GAZEORA_CALL RecorderPanel::Action(void* self, const char* action) {
    Guarded::Run([&] {
        if (std::string(action) == kNewFileAction) {
            static_cast<RecorderPanel*>(self)->m_recording.Restart();
        }
    });
}

std::map<std::string, RecorderPanel::Getter> RecorderPanel::Getters() {
    return {
        {kRecordingKey, [this] { return std::string(m_settings.Recording() ? kOn : kOff); }},
        {kFolderKey, [this] { return m_settings.Folder(); }},
        {kStatusKey, [this] { return m_recording.Status().text; }},
        {kToneKey, [this] { return m_recording.Status().tone; }},
        {kFileOpenKey, [this] { return std::string(m_recording.FileName().empty() ? kOff : kOn); }},
    };
}

std::map<std::string, RecorderPanel::Setter> RecorderPanel::Setters() {
    return {
        {kRecordingKey,
         [this](const std::string& value) {
             m_settings.SetRecording(value == kOn);
             m_recording.Restart();
         }},
        // Set on every key typed, so it waits for the next file rather than starting one.
        {kFolderKey, [this](const std::string& value) { m_settings.SetFolder(value); }},
    };
}

}  // namespace frame_recorder
