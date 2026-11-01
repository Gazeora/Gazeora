// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "sample_parts.h"

namespace sample {

namespace {

constexpr uint32_t kTypeMask = 0x1F;
constexpr uint32_t kIdr = 5;
constexpr uint32_t kSps = 7;
constexpr uint32_t kPps = 8;
constexpr uint32_t kDelimiter = 9;
constexpr uint8_t kFuA = 28;
constexpr uint8_t kNriMask = 0xE0;
constexpr uint8_t kDelimiterBytes[] = {0, 0, 0, 1, 0x09, 0xF0};
constexpr uint8_t kSeiHeader[] = {0x06};

uint32_t GAZEORA_CALL NalType(void*, const uint8_t* nal, size_t size) {
    return size ? nal[0] & kTypeMask : 0;
}

int32_t GAZEORA_CALL IsKeyFrame(void* self, const uint8_t* nal, size_t size) {
    return NalType(self, nal, size) == kIdr ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL IsPicture(void* self, const uint8_t* nal, size_t size) {
    const uint32_t type = NalType(self, nal, size);
    return type >= 1 && type <= kIdr ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL IsDelimiter(void* self, const uint8_t* nal, size_t size) {
    return size && NalType(self, nal, size) == kDelimiter ? GAZEORA_TRUE : GAZEORA_FALSE;
}

uint32_t GAZEORA_CALL ParameterSetOf(void* self, const uint8_t* nal, size_t size) {
    const uint32_t type = NalType(self, nal, size);
    return type == kSps   ? GAZEORA_PARAMETER_SET_SPS
           : type == kPps ? GAZEORA_PARAMETER_SET_PPS
                          : GAZEORA_PARAMETER_SET_NONE;
}

void GAZEORA_CALL Fmtp(void*, const GazeoraBytesV2* sets, char* fmtp, size_t capacity) {
    Copy(sets[GAZEORA_PARAMETER_SET_SPS].size ? "sample=1" : "", fmtp, capacity);
}

size_t GAZEORA_CALL FragmentHeader(void*, const uint8_t* nal, size_t size, uint8_t* header,
                                   size_t capacity) {
    if (!size || !capacity) {
        return 0;
    }
    header[0] = uint8_t((nal[0] & kNriMask) | kFuA);
    return 1;
}

const GazeoraCodecV2 kCodec = {
    sizeof(GazeoraCodecV2),
    nullptr,
    kCodecId,
    "Sample codec",
    "Sample codec - for the tests",
    100,
    "SAMPLE",
    1,
    NalType,
    IsKeyFrame,
    IsPicture,
    IsDelimiter,
    ParameterSetOf,
    {GAZEORA_PARAMETER_SET_SPS, GAZEORA_PARAMETER_SET_PPS, GAZEORA_PARAMETER_SET_NONE},
    Fmtp,
    FragmentHeader,
    nullptr,  // no depacketizer
    0x1B,
    {kDelimiterBytes, sizeof(kDelimiterBytes)},
    {kSeiHeader, sizeof(kSeiHeader)},
};

}  // namespace

const GazeoraCodecV2& Codec() { return kCodec; }

}  // namespace sample
