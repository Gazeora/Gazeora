// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <cstring>

#include "sample_parts.h"

namespace sample {

namespace {

constexpr unsigned char kAccessUnit[] = {0, 0, 0, 1, 0x65, 0x88};

struct SampleEncoder {
    bool open = false;
};

bool IsSampleCodec(const GazeoraEncoderSettingsV1* settings) {
    if (settings->struct_size < sizeof(GazeoraEncoderSettingsV2)) {
        return false;  // a version 1 host
    }
    const auto* v2 = reinterpret_cast<const GazeoraEncoderSettingsV2*>(settings);
    return v2->codec_id && std::strcmp(v2->codec_id, kCodecId) == 0;
}

int32_t GAZEORA_CALL Open(void* self, const GazeoraEncoderSettingsV1* settings, char* error,
                          size_t capacity) {
    const bool codec = settings->codec == GAZEORA_CODEC_H264 || IsSampleCodec(settings);
    if (!codec || settings->d3d11_device) {
        Copy("only H.264 or the sample codec, from memory", error, capacity);
        return GAZEORA_FALSE;
    }
    static_cast<SampleEncoder*>(self)->open = true;
    return GAZEORA_TRUE;
}

void GAZEORA_CALL Close(void* self) { static_cast<SampleEncoder*>(self)->open = false; }
int32_t GAZEORA_CALL IsHardware(void*) { return GAZEORA_FALSE; }
void GAZEORA_CALL SetBitrate(void*, uint32_t) {}
void GAZEORA_CALL RequestKeyFrame(void*) {}

int32_t GAZEORA_CALL Encode(void*, const uint8_t*, int64_t time, GazeoraFrameSinkV1 sink,
                            void* sinkContext, char*, size_t) {
    GazeoraEncodedFrameV1 frame = {};
    frame.struct_size = sizeof(frame);
    frame.annexb = kAccessUnit;
    frame.size = sizeof(kAccessUnit);
    frame.time = time;
    frame.key_frame = GAZEORA_TRUE;
    sink(sinkContext, &frame);
    return GAZEORA_TRUE;
}

void GAZEORA_CALL Destroy(void* self) { delete static_cast<SampleEncoder*>(self); }

int32_t GAZEORA_CALL Supports(void*, uint32_t codec) {
    return codec == GAZEORA_CODEC_H264 ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL SupportsId(void*, const char* codecId) {
    return std::strcmp(codecId, "h264") == 0 || std::strcmp(codecId, kCodecId) == 0 ? GAZEORA_TRUE
                                                                                    : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL Create(void*, GazeoraEncoderV1* encoder) {
    encoder->struct_size = sizeof(*encoder);
    encoder->self = new SampleEncoder();
    encoder->open = Open;
    encoder->close = Close;
    encoder->is_hardware = IsHardware;
    encoder->set_bitrate = SetBitrate;
    encoder->request_key_frame = RequestKeyFrame;
    encoder->encode = Encode;
    encoder->encode_texture = nullptr;
    encoder->destroy = Destroy;
    return GAZEORA_TRUE;
}

const GazeoraEncoderProviderV1 kVersion1 = {sizeof(GazeoraEncoderProviderV1), nullptr,
                                            "Sample encoder", Supports, Create};
const GazeoraEncoderProviderV2 kVersion2 = {
    {sizeof(GazeoraEncoderProviderV2), nullptr, "Sample encoder", Supports, Create}, SupportsId};

}  // namespace

const GazeoraEncoderProviderV1& Encoders(bool withIds) {
    return withIds ? kVersion2.v1 : kVersion1;
}

}  // namespace sample
