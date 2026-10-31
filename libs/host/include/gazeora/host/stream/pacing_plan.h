// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>

namespace gazeora {

// How to spread one frame's UDP packets: in bursts kBurstIntervalUs apart. Sent all at once, a
// key frame overflows Wi-Fi access point and phone buffers; spread wide, a frame arrives late.
// So a frame goes out at kRateFactor times the stream's bitrate, and within half a frame
// interval however big it is: a 1080p frame at 20 Mbit/s in about 10 ms, a key frame in 16 at
// 30 fps or 8 at 60.
class PacingPlan {
public:
    static constexpr uint32_t kRateFactor = 4;
    static constexpr size_t kMinPacketsPerBurst = 4;
    static constexpr int64_t kBurstIntervalUs = 1000;

    // The plan for `packets` packets of `bytes` in all, on a stream of `bitrate` bits/s at
    // `fps` frames a second.
    static PacingPlan For(size_t packets, size_t bytes, uint32_t bitrate, uint32_t fps);
    // Everything in one burst: for a few packets that need no spreading.
    static PacingPlan AtOnce(size_t packets);

    size_t PacketsPerBurst() const { return m_packetsPerBurst; }
    size_t Bursts() const { return m_bursts; }
    // From the first burst to the last.
    int64_t SpreadUs() const { return int64_t(m_bursts > 0 ? m_bursts - 1 : 0) * kBurstIntervalUs; }

private:
    PacingPlan(size_t packetsPerBurst, size_t bursts)
        : m_packetsPerBurst(packetsPerBurst), m_bursts(bursts) {}

    size_t m_packetsPerBurst;
    size_t m_bursts;
};

}  // namespace gazeora
