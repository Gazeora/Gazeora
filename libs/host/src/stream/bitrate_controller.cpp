// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/bitrate_controller.h"

#include <algorithm>

namespace gazeora {

constexpr uint32_t BitrateController::kLadderPercent[];

void BitrateController::Report(uint64_t source, float fractionLost, uint64_t nowMs) {
    m_reports[source] = {fractionLost, nowMs};
}

bool BitrateController::Update(uint64_t nowMs, int viewers) {
    float worst = 0;
    for (auto it = m_reports.begin(); it != m_reports.end();) {
        if (nowMs - it->second.atMs > kLossWindowMs) {
            it = m_reports.erase(it);
        } else {
            worst = std::max(worst, it->second.fraction);
            ++it;
        }
    }
    m_worstLoss = worst;
    int step = m_step;
    if (viewers == 0) {
        step = 0;
    } else if (worst >= kLossToBackOff) {
        m_lastLossMs = nowMs;
        if (nowMs - m_lastChangeMs >= kBackOffEveryMs && step < kSteps - 1) {
            ++step;
            m_lastChangeMs = nowMs;
        }
    } else if (nowMs - m_lastLossMs >= kRecoverAfterMs &&
               nowMs - m_lastChangeMs >= kRecoverEveryMs && step > 0) {
        --step;
        m_lastChangeMs = nowMs;
    }
    bool changed = step != m_step;
    m_step = step;
    return changed;
}

}  // namespace gazeora
