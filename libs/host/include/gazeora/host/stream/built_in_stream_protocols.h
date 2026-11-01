// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/stream_protocol_catalog.h"

namespace gazeora {

// The protocols the stream server serves itself: RTSP over TCP and over UDP. The others come
// from plugins (bundled_protocols.h).
class BuiltInStreamProtocols final {
public:
    BuiltInStreamProtocols() = delete;

    static void RegisterInto(StreamProtocolCatalog& catalog);
    // A catalog with only these.
    static std::shared_ptr<const StreamProtocolCatalog> Catalog();
};

}  // namespace gazeora
