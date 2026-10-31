// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/pointer_gate.h"

namespace gazeora {

void PointerGate::OnPointer(const PointerEvent& event) {
    PointerEventSink* target = m_target;
    if (target) {
        target->OnPointer(event);
    }
}

}  // namespace gazeora
