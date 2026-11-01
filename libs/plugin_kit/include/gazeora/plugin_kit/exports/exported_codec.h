// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "gazeora/host/encode/video_codec_info.h"
#include "gazeora/host/stream/video_codec_format.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: one of Gazeora's own codecs, a VideoCodecInfo and its VideoCodecFormat, as
// a GazeoraCodecV2: what a bundled plugin registers, and what Gazeora hands a plugin output
// with each frame. The struct points back at it, so it can't be copied or moved.
class ExportedCodec final {
public:
    // `info.format` must not be null.
    explicit ExportedCodec(VideoCodecInfo info);
    ExportedCodec(const ExportedCodec&) = delete;
    ExportedCodec& operator=(const ExportedCodec&) = delete;

    const GazeoraCodecV2& Codec() const { return m_codec; }

private:
    static const VideoCodecFormat& FormatOf(void* self);
    static std::string Text(const uint8_t* data, size_t size);

    static uint32_t GAZEORA_CALL NalType(void* self, const uint8_t* nal, size_t size);
    static int32_t GAZEORA_CALL IsKeyFrame(void* self, const uint8_t* nal, size_t size);
    static int32_t GAZEORA_CALL IsPicture(void* self, const uint8_t* nal, size_t size);
    static int32_t GAZEORA_CALL IsDelimiter(void* self, const uint8_t* nal, size_t size);
    static uint32_t GAZEORA_CALL ParameterSetOf(void* self, const uint8_t* nal, size_t size);
    static void GAZEORA_CALL Fmtp(void* self, const GazeoraBytesV2* sets, char* fmtp,
                                  size_t capacity);
    static size_t GAZEORA_CALL FragmentHeader(void* self, const uint8_t* nal, size_t size,
                                              uint8_t* header, size_t capacity);
    static int32_t GAZEORA_CALL Depacketize(void* self, const uint8_t* payload, size_t size,
                                            int32_t* inFragment, GazeoraBytesSinkV2 sink,
                                            void* sinkContext);

    void FillDescription();
    void FillCalls();

    const VideoCodecInfo m_info;
    const std::string m_id;
    const std::string m_seiHeader;
    GazeoraCodecV2 m_codec = {};
};

}  // namespace gazeora
