// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// How frames leave the PC, by its id: the one in stream.ini and in the Android app's discovery
// reply. One at a time: the stream server runs the chosen one only. Its name and help are in a
// StreamProtocolCatalog (stream_protocol_catalog.h).
class StreamProtocol final {
public:
    explicit StreamProtocol(std::string id) : m_id(std::move(id)) {}

    static const StreamProtocol RtspTcp;  // RTSP, RTP interleaved in the RTSP connection
    static const StreamProtocol RtspUdp;  // RTSP, RTP over UDP
    static const StreamProtocol Udp;      // encoded frames in UDP datagrams (frame_packet.h)
    static const StreamProtocol Http;     // a live MPEG-TS stream over HTTP
    static const StreamProtocol Usb;      // to an Android phone in USB accessory mode

    const std::string& Id() const { return m_id; }
    // Whether the server handles it itself, rather than through a FrameOutput.
    bool IsRtsp() const { return *this == RtspTcp || *this == RtspUdp; }
    bool operator==(const StreamProtocol& other) const { return m_id == other.m_id; }
    bool operator!=(const StreamProtocol& other) const { return m_id != other.m_id; }
    bool operator<(const StreamProtocol& other) const { return m_id < other.m_id; }

private:
    std::string m_id;
};

inline const StreamProtocol StreamProtocol::RtspTcp{"rtsp-tcp"};
inline const StreamProtocol StreamProtocol::RtspUdp{"rtsp-udp"};
inline const StreamProtocol StreamProtocol::Udp{"udp"};
inline const StreamProtocol StreamProtocol::Http{"http"};
inline const StreamProtocol StreamProtocol::Usb{"usb"};

}  // namespace gazeora
