// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "remote/pointer_event_queue.h"

namespace gazeora::app {

namespace {

// Touches beyond this while the UI isn't taking them (it hung) are dropped.
constexpr size_t kMaxQueued = 512;

}  // namespace

void PointerEventQueue::OnPointer(const PointerEvent& event) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_events.size() >= kMaxQueued) {
            return;
        }
        m_events.push_back(event);
    }
    m_wake();
}

std::vector<PointerEvent> PointerEventQueue::Take() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PointerEvent> taken;
    taken.swap(m_events);
    return taken;
}

}  // namespace gazeora::app
