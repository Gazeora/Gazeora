// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/built_in_stream_protocols.h"

namespace gazeora {

namespace {

constexpr char kRootPath[] = "/";

}  // namespace

void BuiltInStreamProtocols::RegisterInto(StreamProtocolCatalog& catalog) {
    catalog.Register({StreamProtocol::RtspTcp, "RTSP over TCP",
                      "RTSP over TCP: works with the Gazeora app, VLC, ffplay and OBS. Reliable: "
                      "nothing is lost, and a slow connection skips frames instead. Players set "
                      "to UDP can't connect.",
                      kRootPath, true});
    catalog.Register({StreamProtocol::RtspUdp, "RTSP over UDP",
                      "RTSP over UDP: works with VLC, ffplay and OBS. The lowest delay on a good "
                      "network, but a lost packet briefly smears the picture. Doesn't work with "
                      "Android emulators.",
                      kRootPath, true});
}

std::shared_ptr<const StreamProtocolCatalog> BuiltInStreamProtocols::Catalog() {
    auto catalog = std::make_shared<StreamProtocolCatalog>();
    RegisterInto(*catalog);
    return catalog;
}

}  // namespace gazeora
