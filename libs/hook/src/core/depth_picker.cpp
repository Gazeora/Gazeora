// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/depth_picker.h"

namespace gazeora::hook {

namespace {

// The back buffer's shape to within 3%, and at least a third of its width: dynamic
// resolution renders smaller, but not that much smaller.
bool HasShapeOf(const DepthPicker::Candidate& c, UINT width, UINT height) {
    if (c.width == 0 || c.height == 0 || width == 0 || height == 0 || c.width * 3 < width) {
        return false;
    }
    double aspect = double(c.width) / c.height, target = double(width) / height;
    return aspect > target * 0.97 && aspect < target * 1.03;
}

}  // namespace

void DepthPicker::See(const void* id, const Candidate& candidate) {
    m_frame.emplace(id, Entry{candidate, 0});
}

void DepthPicker::Count(const void* id, uint32_t draws) {
    auto it = m_frame.find(id);
    if (it != m_frame.end()) {
        it->second.draws += draws;
    }
}

const void* DepthPicker::Pick(UINT backBufferWidth, UINT backBufferHeight) {
    const void* best = nullptr;
    uint64_t bestDraws = 0, pickedDraws = 0;
    for (const auto& [id, entry] : m_frame) {
        if (!entry.candidate.usable || entry.draws == 0 ||
            !HasShapeOf(entry.candidate, backBufferWidth, backBufferHeight)) {
            continue;
        }
        if (entry.draws > bestDraws) {
            best = id;
            bestDraws = entry.draws;
        }
        if (id == m_picked) {
            pickedDraws = entry.draws;
        }
    }
    m_frame.clear();
    // Clearly more: half as many draws again.
    if (!best || best == m_picked || (pickedDraws > 0 && pickedDraws * 3 >= bestDraws * 2)) {
        m_challenger = nullptr;
        m_challengerFrames = 0;
        return m_picked;
    }
    if (!m_picked) {
        m_picked = best;  // the first: nothing to keep steady yet
        return m_picked;
    }
    m_challengerFrames = best == m_challenger ? m_challengerFrames + 1 : 1;
    m_challenger = best;
    if (m_challengerFrames >= kFramesToSwitch) {
        m_picked = best;
        m_challenger = nullptr;
        m_challengerFrames = 0;
    }
    return m_picked;
}

void DepthPicker::Clear() {
    m_frame.clear();
    m_picked = nullptr;
    m_challenger = nullptr;
    m_challengerFrames = 0;
}

}  // namespace gazeora::hook
