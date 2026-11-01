// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/frame_output_factory.h"

namespace gazeora {

// Factory Method pattern: the factory for an output constructed from the log alone.
template <typename Output>
class SimpleFrameOutputFactory final : public FrameOutputFactory {
public:
    explicit SimpleFrameOutputFactory(StreamProtocol protocol) : m_protocol(protocol) {}

    StreamProtocol Protocol() const override { return m_protocol; }
    std::unique_ptr<FrameOutput> Create(StreamLog& log) const override {
        return std::make_unique<Output>(log);
    }

private:
    const StreamProtocol m_protocol;
};

}  // namespace gazeora
