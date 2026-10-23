#include "gazeora/host/stream/pointer_gate.h"

namespace gazeora {

void PointerGate::OnPointer(const PointerEvent& event) {
    PointerEventSink* target = m_target;
    if (target && !m_vrOn) {
        target->OnPointer(event);
    }
}

}  // namespace gazeora
