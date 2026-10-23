#include "gazeora/host/stream/pacing_plan.h"

#include <algorithm>

namespace gazeora {

namespace {

// A bitrate the stream never goes below, for a frame whose stream says none.
constexpr uint64_t kMinBitrate = 2000000;
constexpr int64_t kMicrosecondsPerSecond = 1000000;

}  // namespace

PacingPlan PacingPlan::For(size_t packets, size_t bytes, uint32_t bitrate, uint32_t fps) {
    if (packets == 0) {
        return PacingPlan(kMinPacketsPerBurst, 0);
    }
    uint64_t rate = std::max<uint64_t>(bitrate, kMinBitrate) * kRateFactor;
    int64_t spreadUs = int64_t(uint64_t(bytes) * 8 * kMicrosecondsPerSecond / rate);
    int64_t maxSpreadUs = kMicrosecondsPerSecond / std::max<uint32_t>(fps, 1) / 2;
    spreadUs = std::min(spreadUs, maxSpreadUs);
    size_t bursts = std::max<size_t>(1, size_t((spreadUs + kBurstIntervalUs - 1) / kBurstIntervalUs));
    size_t perBurst = std::max(kMinPacketsPerBurst, (packets + bursts - 1) / bursts);
    return PacingPlan(perBurst, (packets + perBurst - 1) / perBurst);
}

PacingPlan PacingPlan::AtOnce(size_t packets) {
    return PacingPlan(std::max<size_t>(packets, 1), packets > 0 ? 1 : 0);
}

}  // namespace gazeora
