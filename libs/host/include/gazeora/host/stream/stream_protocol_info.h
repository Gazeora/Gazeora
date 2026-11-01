// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/host/stream/stream_protocol.h"

namespace gazeora {

// What the UI and the discovery reply need to know about a protocol.
struct StreamProtocolInfo {
    StreamProtocol id;
    std::string name;  // for people, e.g. "RTSP over TCP"
    std::string help;  // what it's good for, in the settings' help
    // The path the Android app opens, sent in the discovery reply.
    std::string discoveryPath = "/";
    // Whether players such as VLC open it by its URL (for the help on their buffering).
    bool openedByPlayers = false;
};

}  // namespace gazeora
