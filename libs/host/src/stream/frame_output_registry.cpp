// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/frame_output_registry.h"

#include <stdexcept>

namespace gazeora {

void FrameOutputRegistry::Register(std::shared_ptr<const FrameOutputFactory> factory) {
    if (!factory) {
        throw std::invalid_argument("FrameOutputRegistry: null factory");
    }
    const StreamProtocol protocol = factory->Protocol();
    m_factories[protocol] = std::move(factory);
}

bool FrameOutputRegistry::Has(StreamProtocol protocol) const {
    return m_factories.count(protocol) > 0;
}

std::unique_ptr<FrameOutput> FrameOutputRegistry::Create(StreamProtocol protocol,
                                                         StreamLog& log) const {
    auto found = m_factories.find(protocol);
    return found == m_factories.end() ? nullptr : found->second->Create(log);
}

}  // namespace gazeora
