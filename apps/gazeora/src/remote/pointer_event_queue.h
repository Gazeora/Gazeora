#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "gazeora/host/stream/pointer_event_sink.h"

namespace gazeora::app {

// Hands the phone's touches from the stream's threads to the UI thread, which alone may feed
// them to the UI, and wakes it so they show at once.
class PointerEventQueue final : public PointerEventSink {
public:
    // `wake` is called from the stream's threads after each touch.
    explicit PointerEventQueue(std::function<void()> wake) : m_wake(std::move(wake)) {}

    void OnPointer(const PointerEvent& event) override;
    // The touches since the last call, oldest first.
    std::vector<PointerEvent> Take();

private:
    std::function<void()> m_wake;
    std::mutex m_mutex;
    std::vector<PointerEvent> m_events;
};

}  // namespace gazeora::app
