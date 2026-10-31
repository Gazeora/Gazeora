// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>

#include "gazeora/host/stream/pointer_event_sink.h"

namespace gazeora {

// Decorator pattern: passes pointer events on to a target, once there is one. They come in VR
// too: the phone's touches only while the picture is flat, and in a VR viewer its hand
// pointing at this window, both on the window's own spots.
class PointerGate final : public PointerEventSink {
public:
    // Null drops every event. Set before the stream starts.
    void SetTarget(PointerEventSink* target) { m_target = target; }

    void OnPointer(const PointerEvent& event) override;

private:
    std::atomic<PointerEventSink*> m_target{nullptr};
};

}  // namespace gazeora
