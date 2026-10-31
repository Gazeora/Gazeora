// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "gazeora/host/stream/head_pose.h"

namespace gazeora {

// The payload of a GZF1 HeadPose message (frame_packet.h), big-endian, after the header whose
// last word is the payload length (over UDP and USB alike), to kPointerPort over the network:
//    0  u32  sample
//    4  i16  x, y, z, w: the quaternion, each -1..1 as -32767..32767
// The Android app has its own copy (protocols/head_pose_message.h); keep the two in step.
constexpr size_t kHeadPosePayloadSize = 12;

// Head pose messages to and from bytes.
class HeadPoseMessage {
public:
    // False if the payload is short or its quaternion is far from unit length.
    static bool Read(const uint8_t* payload, size_t size, HeadPose& out);
    // A whole message, header and payload, as the app sends it (for tests).
    static std::string Write(const HeadPose& pose);

private:
    static constexpr size_t kSampleAt = 0;
    static constexpr size_t kQuaternionAt = 4;
    static constexpr size_t kComponentSize = 2;
};

}  // namespace gazeora
