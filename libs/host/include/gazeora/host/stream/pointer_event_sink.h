#pragma once

#include "gazeora/host/stream/pointer_event.h"

namespace gazeora {

// Where pointer events from the phone go. Called from the stream's threads.
class PointerEventSink {
public:
    virtual ~PointerEventSink() = default;
    virtual void OnPointer(const PointerEvent& event) = 0;
};

}  // namespace gazeora
