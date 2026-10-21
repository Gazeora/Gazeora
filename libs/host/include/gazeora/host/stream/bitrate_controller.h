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

    // Bits/s from the top of the ladder down. `percent` scales the whole ladder (H.265 gets
    // about the same picture from 70% of H.264's bits).
    static constexpr uint32_t kLadder[] = {8000000, 6000000, 4500000, 3000000};
    static constexpr int kSteps = int(sizeof(kLadder) / sizeof(kLadder[0]));

    // `source` identifies a player (its address, or its connection); a newer report from the
    // same one replaces the older.
    void Report(uint64_t source, float fractionLost, uint64_t nowMs);
    // Drops stale reports and moves along the ladder. Without viewers it goes back to the
    // top, so the next player starts at full quality. True if the step changed.
    bool Update(uint64_t nowMs, int viewers);

    int Step() const { return m_step; }  // 0 is the top
    float WorstLoss() const { return m_worstLoss; }
    static uint32_t BitrateAt(int step, uint32_t percent) {
        return kLadder[step] / 100 * percent;
    }

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
