#pragma once

#include <string>

namespace gazeora {

// How frames leave the PC. One at a time: the stream server runs the chosen one only.
enum class StreamProtocol {
    RtspTcp,  // RTSP, RTP interleaved in the RTSP connection
    RtspUdp,  // RTSP, RTP over UDP
    Udp,      // encoded frames in UDP datagrams, no RTSP or RTP (frame_packet.h)
    Http,     // a live MPEG-TS stream over HTTP
    Usb,      // encoded frames to an Android phone in USB accessory mode (frame_packet.h)
};

inline constexpr StreamProtocol kAllStreamProtocols[] = {
    StreamProtocol::RtspTcp, StreamProtocol::RtspUdp, StreamProtocol::Udp,
    StreamProtocol::Http,    StreamProtocol::Usb,
};

// The protocol's id in stream.ini and in discovery replies: "rtsp-tcp", "rtsp-udp", "udp",
// "http", "usb".
const char* ProtocolId(StreamProtocol protocol);
// For people: "RTSP over TCP".
const char* ProtocolName(StreamProtocol protocol);
// False, leaving `protocol` alone, for an unknown id.
bool ParseProtocolId(const std::string& id, StreamProtocol& protocol);

}  // namespace gazeora
