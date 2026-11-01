// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recorder_output.h"

#include "guarded.h"
#include "text_buffer.h"

namespace frame_recorder {

RecorderOutput::RecorderOutput(Recording& recording, const GazeoraOutputHostV2& host)
    : m_recording(recording), m_host(host) {}

GazeoraOutputV2 RecorderOutput::Struct() {
    GazeoraOutputV2 output = {};
    output.struct_size = sizeof(GazeoraOutputV2);
    output.self = this;
    output.start = Start;
    output.stop = Stop;
    output.port = Port;
    output.viewers = Viewers;
    output.take_key_frame_request = TakeKeyFrameRequest;
    output.send = Send;
    output.url = Url;
    output.status = Status;
    output.destroy = Destroy;
    // app_viewers, watching and the USB functions are optional: NULL.
    return output;
}

RecorderOutput& RecorderOutput::Of(void* self) { return *static_cast<RecorderOutput*>(self); }

int32_t GAZEORA_CALL RecorderOutput::Start(void* self, uint32_t, uint16_t, char*, size_t) {
    // A file takes no players, so the address and port Gazeora offers go unused.
    Guarded::Run([&] { Of(self).m_recording.Begin(); });
    return GAZEORA_TRUE;
}

void GAZEORA_CALL RecorderOutput::Stop(void* self) {
    Guarded::Run([&] { Of(self).m_recording.End(); });
}

uint16_t GAZEORA_CALL RecorderOutput::Port(void*) { return 0; }

// Nobody watches a file. With no viewers, Gazeora sends an output a game's frames but not its
// own window's, so only games are recorded.
int32_t GAZEORA_CALL RecorderOutput::Viewers(void*) { return 0; }

int32_t GAZEORA_CALL RecorderOutput::TakeKeyFrameRequest(void* self) {
    return Of(self).m_recording.TakeKeyFrameRequest() ? GAZEORA_TRUE : GAZEORA_FALSE;
}

void GAZEORA_CALL RecorderOutput::Send(void* self, const GazeoraFrameV2* frame,
                                       const GazeoraLossReporterV2*) {
    // No player loses packets, so there's no loss to report.
    Guarded::Run([&] {
        RecorderOutput& output = Of(self);
        switch (output.m_recording.Write(*frame)) {
            case Recording::WriteResult::Opened:
                output.Log("recording to " + output.m_recording.FileName());
                break;
            case Recording::WriteResult::Failed:
                output.Log("can't write the recording: " + output.m_recording.Status().text);
                break;
            default:
                break;
        }
    });
}

void GAZEORA_CALL RecorderOutput::Url(void*, const char*, char* url, size_t capacity) {
    TextBuffer::Write("", url, capacity);  // nothing for a player to open
}

void GAZEORA_CALL RecorderOutput::Status(void* self, char* status, size_t capacity) {
    Guarded::Run([&] { TextBuffer::Write(Of(self).m_recording.Status().text, status, capacity); });
}

void GAZEORA_CALL RecorderOutput::Destroy(void* self) { delete &Of(self); }

void RecorderOutput::Log(const std::string& line) const {
    m_host.log(m_host.context, line.c_str());
}

}  // namespace frame_recorder
