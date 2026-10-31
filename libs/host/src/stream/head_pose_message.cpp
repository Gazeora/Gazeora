// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_pose_message.h"

#include <algorithm>
#include <cmath>

#include "gazeora/host/stream/frame_packet.h"

namespace gazeora {

namespace {

constexpr float kComponentRange = 32767.0f;
// Rounding to 16 bits moves the length a little; garbage moves it a lot.
constexpr float kUnitTolerance = 0.01f;

uint16_t Get16(const uint8_t* in) { return uint16_t(in[0] << 8 | in[1]); }

uint32_t Get32(const uint8_t* in) {
    return uint32_t(in[0]) << 24 | uint32_t(in[1]) << 16 | uint32_t(in[2]) << 8 | in[3];
}

void Put16(uint8_t* out, uint16_t value) {
    out[0] = uint8_t(value >> 8);
    out[1] = uint8_t(value);
}

void Put32(uint8_t* out, uint32_t value) {
    out[0] = uint8_t(value >> 24);
    out[1] = uint8_t(value >> 16);
    out[2] = uint8_t(value >> 8);
    out[3] = uint8_t(value);
}

float Component(const uint8_t* in) { return int16_t(Get16(in)) / kComponentRange; }

uint16_t Encode(float component) {
    return uint16_t(int16_t(std::lround(std::clamp(component, -1.0f, 1.0f) * kComponentRange)));
}

}  // namespace

bool HeadPoseMessage::Read(const uint8_t* payload, size_t size, HeadPose& out) {
    if (size < kHeadPosePayloadSize) {
        return false;
    }
    const uint8_t* q = payload + kQuaternionAt;
    HeadPose pose;
    pose.sample = Get32(payload + kSampleAt);
    pose.x = Component(q);
    pose.y = Component(q + kComponentSize);
    pose.z = Component(q + 2 * kComponentSize);
    pose.w = Component(q + 3 * kComponentSize);
    float length = std::sqrt(pose.x * pose.x + pose.y * pose.y + pose.z * pose.z + pose.w * pose.w);
    if (std::fabs(length - 1.0f) > kUnitTolerance) {
        return false;
    }
    out = pose;
    return true;
}

std::string HeadPoseMessage::Write(const HeadPose& pose) {
    uint8_t payload[kHeadPosePayloadSize];
    uint8_t* q = payload + kQuaternionAt;
    Put32(payload + kSampleAt, pose.sample);
    Put16(q, Encode(pose.x));
    Put16(q + kComponentSize, Encode(pose.y));
    Put16(q + 2 * kComponentSize, Encode(pose.z));
    Put16(q + 3 * kComponentSize, Encode(pose.w));
    FrameHeader header;
    header.type = FrameMessage::HeadPose;
    return StreamMessage(header, std::string(reinterpret_cast<char*>(payload), sizeof(payload)));
}

}  // namespace gazeora
