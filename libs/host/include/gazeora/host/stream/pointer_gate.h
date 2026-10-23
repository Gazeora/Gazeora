#pragma once

#include <atomic>

#include "gazeora/host/stream/pointer_event_sink.h"

namespace gazeora {

// Decorator pattern: passes pointer events on to a target, but not while the stream is laid
// out for a VR viewer, where a touch has no spot in the picture to land on.
class PointerGate final : public PointerEventSink {
public:
    // Null drops every event. Set before the stream starts.
    void SetTarget(PointerEventSink* target) { m_target = target; }
    void SetVrOn(bool vrOn) { m_vrOn = vrOn; }

    void OnPointer(const PointerEvent& event) override;

private:
    std::atomic<PointerEventSink*> m_target{nullptr};
    std::atomic<bool> m_vrOn{false};
};

}  // namespace gazeora
