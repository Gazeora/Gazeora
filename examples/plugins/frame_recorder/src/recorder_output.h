// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "recording.h"

namespace frame_recorder {

// Adapter pattern: the GazeoraOutputV2 that runs while Record to file is the stream's protocol,
// onto the Recording. Gazeora frees it through `destroy`.
class RecorderOutput {
public:
    RecorderOutput(Recording& recording, const GazeoraOutputHostV2& host);

    // This object behind the C functions Gazeora calls.
    GazeoraOutputV2 Struct();

private:
    static RecorderOutput& Of(void* self);
    static int32_t GAZEORA_CALL Start(void* self, uint32_t ipv4, uint16_t port, char* error,
                                      size_t capacity);
    static void GAZEORA_CALL Stop(void* self);
    static uint16_t GAZEORA_CALL Port(void* self);
    static int32_t GAZEORA_CALL Viewers(void* self);
    static int32_t GAZEORA_CALL TakeKeyFrameRequest(void* self);
    static void GAZEORA_CALL Send(void* self, const GazeoraFrameV2* frame,
                                  const GazeoraLossReporterV2* loss);
    static void GAZEORA_CALL Url(void* self, const char* host, char* url, size_t capacity);
    static void GAZEORA_CALL Status(void* self, char* status, size_t capacity);
    static void GAZEORA_CALL Destroy(void* self);

    void Log(const std::string& line) const;

    Recording& m_recording;
    GazeoraOutputHostV2 m_host;  // its context stays valid while the output exists
};

}  // namespace frame_recorder
