// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

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
