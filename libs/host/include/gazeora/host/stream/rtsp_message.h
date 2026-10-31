// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gazeora {

// RTSP 1.0 (RFC 2326) messages, as far as the stream server needs them.

struct RtspRequest {
    std::string method;  // "DESCRIBE", "SETUP", ...
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;  // names lowercased
    std::string body;

    // Parses one whole request (as RtspReader hands it out). False if it isn't RTSP.
    static bool Parse(const std::string& text, RtspRequest& out);

    // The value of header `name` (any case), trimmed; empty if absent.
    std::string Header(const char* name) const;
    // The Session header without its parameters (";timeout=...").
    std::string SessionId() const;
};

// Builds a response: status line, CSeq and Server headers, then whatever is added.
class RtspResponse {
public:
    RtspResponse(const char* status, const RtspRequest& request);

    RtspResponse& Header(const std::string& name, const std::string& value);
    RtspResponse& Body(const std::string& contentType, std::string body);
    std::string Serialize() const;

private:
    std::string m_head;
    std::string m_body;
};

// What a SETUP's Transport header asks for.
struct TransportSpec {
    bool interleaved = false;  // RTP inside the RTSP connection
    uint8_t channel = 0;       // interleaved: RTP's channel, RTCP on the next
    uint16_t rtpPort = 0;      // UDP: the player's ports
    uint16_t rtcpPort = 0;

    // False for transports the server doesn't offer (multicast, TCP without interleaving)
    // or that are malformed; the reply is then 461 Unsupported Transport.
    static bool Parse(const std::string& header, TransportSpec& out);
};

// Splits what arrives on an RTSP connection into requests and interleaved binary frames
// ("$", channel, 16-bit length, data), which players send their RTCP reports in.
class RtspReader {
public:
    enum class Item { None, Request, Interleaved, TooLarge };

    void Append(const char* data, size_t size) { m_buffer.append(data, size); }
    // The next complete item: a request's text goes to `out`, an interleaved frame is
    // consumed. None until more arrives; TooLarge when a request outgrows the limit.
    Item Next(std::string& out);

private:
    std::string m_buffer;
};

}  // namespace gazeora
