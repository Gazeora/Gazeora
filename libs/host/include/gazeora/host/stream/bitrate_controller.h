// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstdint>
#include <map>

namespace gazeora {

// Picks the stream's bitrate from the loss players report. While any player reports
// kLossToBackOff or more, the bitrate steps down a ladder, a step every kBackOffEveryMs; once
// the reports have been clean for kRecoverAfterMs it climbs back a step every
// kRecoverEveryMs. Fewer bits mean fewer packets per frame, so fewer frames hit by a loss.
// Pure logic, no clock or network of its own: time comes in as milliseconds, so it can be
// tested directly. Reports and Update come from one thread; the readers from any.
class BitrateController {
public:
    static constexpr float kLossToBackOff = 0.02f;
    // VLC reports about every 5 s, so a report has to count for longer than that.
    static constexpr uint64_t kLossWindowMs = 6000;
    static constexpr uint64_t kBackOffEveryMs = 1500;
    static constexpr uint64_t kRecoverAfterMs = 8000;
    static constexpr uint64_t kRecoverEveryMs = 3000;

    // The ladder, from the top down, in percent of the top bitrate (the stream quality picked,
    // StreamQuality). The bottom still gives a usable picture: a weak link softens it, rather
    // than washing it out.
    static constexpr uint32_t kLadderPercent[] = {100, 75, 55, 40, 28};
    static constexpr int kSteps = int(sizeof(kLadderPercent) / sizeof(kLadderPercent[0]));

    // `source` identifies a player (its address, or its connection); a newer report from the
    // same one replaces the older.
    void Report(uint64_t source, float fractionLost, uint64_t nowMs);
    // Drops stale reports and moves along the ladder. Without viewers it goes back to the
    // top, so the next player starts at full quality. True if the step changed.
    bool Update(uint64_t nowMs, int viewers);

    int Step() const { return m_step; }  // 0 is the top
    float WorstLoss() const { return m_worstLoss; }
    // Bits/s at `step` of a ladder topping out at `top` bits/s.
    static uint32_t BitrateAt(int step, uint32_t top) { return top / 100 * kLadderPercent[step]; }

private:
    struct LossReport {
        float fraction;
        uint64_t atMs;
    };
    std::map<uint64_t, LossReport> m_reports;
    uint64_t m_lastLossMs = 0;
    uint64_t m_lastChangeMs = 0;
    std::atomic<int> m_step{0};
    std::atomic<float> m_worstLoss{0.0f};
};

}  // namespace gazeora
