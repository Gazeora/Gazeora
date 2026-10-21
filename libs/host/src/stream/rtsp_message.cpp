#include "gazeora/host/stream/rtsp_message.h"

#include <algorithm>
#include <cstdlib>

namespace gazeora {

namespace {

constexpr size_t kMaxRequestBytes = 16384;

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(tolower(c)); });
    return text;
}

std::string Trim(const std::string& text) {
    size_t begin = text.find_first_not_of(" \t");
    size_t end = text.find_last_not_of(" \t\r\n");
    return begin == std::string::npos ? std::string() : text.substr(begin, end - begin + 1);
}

// The Content-Length of a request head, 0 if it has none.
size_t ContentLength(const std::string& head) {
    std::string lower = Lower(head);
    size_t at = lower.find("\r\ncontent-length:");
    return at == std::string::npos ? 0 : strtoul(head.c_str() + at + 17, nullptr, 10);
}

}  // namespace

bool RtspRequest::Parse(const std::string& text, RtspRequest& out) {
    size_t lineEnd = text.find("\r\n");
    std::string line = text.substr(0, lineEnd);
    size_t methodEnd = line.find(' ');
    size_t urlEnd = methodEnd == std::string::npos ? methodEnd : line.find(' ', methodEnd + 1);
    if (urlEnd == std::string::npos) {
        return false;
    }
    out = RtspRequest();
    out.method = line.substr(0, methodEnd);
    out.url = line.substr(methodEnd + 1, urlEnd - methodEnd - 1);
    size_t headEnd = text.find("\r\n\r\n");
    for (size_t at = lineEnd; at != std::string::npos && at < headEnd;) {
        at += 2;
        size_t end = text.find("\r\n", at);
        size_t colon = text.find(':', at);
        if (colon != std::string::npos && colon < end) {
            out.headers.emplace_back(Lower(Trim(text.substr(at, colon - at))),
                                     Trim(text.substr(colon + 1, end - colon - 1)));
        }
        at = end;
    }
    if (headEnd != std::string::npos) {
        out.body = text.substr(headEnd + 4);
    }
    return true;
}

std::string RtspRequest::Header(const char* name) const {
    std::string key = Lower(name);
    for (const auto& header : headers) {
        if (header.first == key) return header.second;
    }
    return {};
}

std::string RtspRequest::SessionId() const {
    std::string value = Header("Session");
    return Trim(value.substr(0, value.find(';')));
}

RtspResponse::RtspResponse(const char* status, const RtspRequest& request) {
    m_head = std::string("RTSP/1.0 ") + status + "\r\nCSeq: " + request.Header("CSeq") +
             "\r\nServer: Gazeora\r\n";
}

RtspResponse& RtspResponse::Header(const std::string& name, const std::string& value) {
    m_head += name + ": " + value + "\r\n";
    return *this;
}

RtspResponse& RtspResponse::Body(const std::string& contentType, std::string body) {
    Header("Content-Type", contentType);
    m_body = std::move(body);
    return *this;
}

std::string RtspResponse::Serialize() const {
    std::string text = m_head;
    if (!m_body.empty()) {
        text += "Content-Length: " + std::to_string(m_body.size()) + "\r\n";
    }
    return text + "\r\n" + m_body;
}

bool TransportSpec::Parse(const std::string& header, TransportSpec& out) {
    std::string transport = Lower(header);
    out = TransportSpec();
    if (transport.find("multicast") != std::string::npos) {
        return false;
    }
    size_t interleavedAt = transport.find("interleaved");
    if (interleavedAt != std::string::npos) {
        out.interleaved = true;
        unsigned long channel = 0;
        if (transport.compare(interleavedAt, 12, "interleaved=") == 0) {
            channel = strtoul(transport.c_str() + interleavedAt + 12, nullptr, 10);
        }
        out.channel = uint8_t(channel);
        return channel <= 254;
    }
    size_t portAt = transport.find("client_port=");
    if (transport.find("/tcp") != std::string::npos || portAt == std::string::npos) {
        return false;
    }
    char* rangeEnd = nullptr;
    unsigned long rtpPort = strtoul(transport.c_str() + portAt + 12, &rangeEnd, 10);
    unsigned long rtcpPort = *rangeEnd == '-' ? strtoul(rangeEnd + 1, nullptr, 10) : 0;
    if (rtpPort == 0 || rtpPort > 65534) {
        return false;
    }
    if (rtcpPort == 0 || rtcpPort > 65535) {
        rtcpPort = rtpPort + 1;
    }
    out.rtpPort = uint16_t(rtpPort);
    out.rtcpPort = uint16_t(rtcpPort);
    return true;
}

RtspReader::Item RtspReader::Next(std::string& out) {
    if (!m_buffer.empty() && m_buffer[0] == '$') {
        if (m_buffer.size() >= 4) {
            size_t total = 4 + (uint8_t(m_buffer[2]) << 8 | uint8_t(m_buffer[3]));
            if (m_buffer.size() >= total) {
                m_buffer.erase(0, total);
                return Item::Interleaved;
            }
        }
        return Item::None;
    }
    size_t headEnd = m_buffer.find("\r\n\r\n");
    if (headEnd == std::string::npos) {
        return m_buffer.size() > kMaxRequestBytes ? Item::TooLarge : Item::None;
    }
    size_t total = headEnd + 4 + ContentLength(m_buffer.substr(0, headEnd + 2));
    if (total > kMaxRequestBytes) {
        return Item::TooLarge;
    }
    if (m_buffer.size() < total) {
        return Item::None;
    }
    out = m_buffer.substr(0, total);
    m_buffer.erase(0, total);
    return Item::Request;
}

}  // namespace gazeora
