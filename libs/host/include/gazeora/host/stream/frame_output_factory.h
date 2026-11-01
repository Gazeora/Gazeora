// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/stream_log.h"
#include "gazeora/host/stream/stream_protocol.h"

namespace gazeora {

// Abstract Factory pattern: makes the FrameOutput for one protocol. Registered in a
// FrameOutputRegistry; adding an output means adding a factory, not editing the server.
class FrameOutputFactory {
public:
    virtual ~FrameOutputFactory() = default;

    virtual StreamProtocol Protocol() const = 0;
    // A new, stopped output that writes to `log`, which outlives it.
    virtual std::unique_ptr<FrameOutput> Create(StreamLog& log) const = 0;
};

}  // namespace gazeora
