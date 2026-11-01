// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/frame_output_registry.h"
#include "gazeora/host/stream/stream_protocol_catalog.h"

namespace gazeora {

// The protocols Gazeora's bundled plugins add (plugins/): UDP, HTTP and USB, with their
// outputs. Gazeora.exe gets them from the plugins; tests, tools and benchmarks that run
// without them get them here.
class BundledProtocols final {
public:
    BundledProtocols() = delete;

    static void RegisterInto(StreamProtocolCatalog& catalog, FrameOutputRegistry& outputs);
    // The built-in protocols and the bundled ones.
    static std::shared_ptr<const StreamProtocolCatalog> Catalog();
    // The bundled protocols' outputs.
    static std::shared_ptr<const FrameOutputRegistry> Outputs();
};

}  // namespace gazeora
