// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/stream_protocol.h"

namespace gazeora {

const char* ProtocolId(StreamProtocol protocol) {
    switch (protocol) {
        case StreamProtocol::RtspTcp:
            return "rtsp-tcp";
        case StreamProtocol::RtspUdp:
            return "rtsp-udp";
        case StreamProtocol::Udp:
            return "udp";
        case StreamProtocol::Http:
            return "http";
        case StreamProtocol::Usb:
            return "usb";
    }
    return "rtsp-tcp";
}

const char* ProtocolName(StreamProtocol protocol) {
    switch (protocol) {
        case StreamProtocol::RtspTcp:
            return "RTSP over TCP";
        case StreamProtocol::RtspUdp:
            return "RTSP over UDP";
        case StreamProtocol::Udp:
            return "UDP";
        case StreamProtocol::Http:
            return "HTTP";
        case StreamProtocol::Usb:
            return "USB";
    }
    return "RTSP over TCP";
}

bool ParseProtocolId(const std::string& id, StreamProtocol& protocol) {
    for (StreamProtocol candidate : kAllStreamProtocols) {
        if (id == ProtocolId(candidate)) {
            protocol = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace gazeora
