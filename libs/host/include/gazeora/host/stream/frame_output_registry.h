// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <memory>

#include "gazeora/host/stream/frame_output_factory.h"

namespace gazeora {

// Registry pattern: the FrameOutputFactory for each protocol the server streams through an
// output. RTSP has none: the server handles it itself. Fill it before handing it to the
// server; it's read only after that.
class FrameOutputRegistry final {
public:
    // Replaces the factory registered for the same protocol, if any. Throws
    // std::invalid_argument for a null factory.
    void Register(std::shared_ptr<const FrameOutputFactory> factory);

    bool Has(StreamProtocol protocol) const;
    // A new output for `protocol`; null when no factory is registered for it.
    std::unique_ptr<FrameOutput> Create(StreamProtocol protocol, StreamLog& log) const;

private:
    std::map<StreamProtocol, std::shared_ptr<const FrameOutputFactory>> m_factories;
};

}  // namespace gazeora
