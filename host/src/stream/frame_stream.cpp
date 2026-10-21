#include "gazeora/host/stream/frame_stream.h"

#include <iphlpapi.h>
#include <mfapi.h>
#include <mmsystem.h>
#include <wrl/client.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>

#include "gazeora/host/stream/rtp_video.h"
#include "gazeora/host/encode/video_encoder.h"

// Missing from older MinGW headers.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

// While players report this much loss the bitrate steps down this ladder, and it climbs
// back once the reports have been clean for a while. Fewer bits mean fewer packets per
// frame, so fewer frames hit by a loss.
constexpr uint32_t kBitrates[] = {8000000, 6000000, 4500000, 3000000};
constexpr int kLowestLevel = int(sizeof(kBitrates) / sizeof(kBitrates[0])) - 1;
// H.265 gets about the same picture from 70% of the bits. The rest is saved rather than
// spent on detail: fewer packets per frame means fewer frames hit by a loss on Wi-Fi.
constexpr uint32_t kH265BitratePercent = 70;
constexpr uint32_t kFrameRate = 30;
// A key frame every second: what a lost packet smears is repaired by then, and a player
// that joins without one still starts within a second. More often costs too much picture
// quality at these bitrates.
constexpr uint32_t kKeyFrameInterval = 30;
constexpr float kLossToBackOff = 0.02f;
// VLC reports about every 5 s, so a report has to count for longer than that.
constexpr uint64_t kLossWindowMs = 6000;
constexpr uint64_t kBackOffEveryMs = 1500;
constexpr uint64_t kRecoverAfterMs = 8000;
constexpr uint64_t kRecoverEveryMs = 3000;
// A frame's packets are spread over about kSpreadMs, in bursts a millisecond apart of at
// least kMinPacketsPerBurst: a 20-packet frame takes 5 ms, a 150-packet key frame 19.
// Sent all at once, the packets of a key frame overflow Wi-Fi access point and phone
// buffers; spread wider, they'd cost latency.
constexpr size_t kMinPacketsPerBurst = 4;
constexpr size_t kSpreadMs = 20;
constexpr LONGLONG kBurstIntervalUs = 1000;
// Interleaved players: a frame is skipped while this much is still queued for the player,
// about a quarter second at the full bitrate. The socket's own buffer is kept small too, so
// what's queued stays where it can be counted.
constexpr size_t kTcpBacklogBytes = 256 * 1024;
constexpr int kTcpSendBufferBytes = 128 * 1024;
// A UDP player without a receiver report for this long is noted in the log. VLC sends one
// every few seconds, so a player that stops has most likely stopped receiving.
constexpr uint64_t kQuietPlayerMs = 12000;
constexpr long kMaxLogBytes = 4 << 20;
// RTP packets stay under a typical 1500-byte Ethernet / Wi-Fi MTU.
constexpr size_t kMaxPacketSize = 1400;
// Without a live frame for this long, the idle frame is streamed, kIdleIntervalMs apart:
// the full frame rate, as players (VLC at least) drop frames as late when a stream runs much
// slower than 30 fps. A repeated still picture costs next to nothing.
constexpr uint64_t kLiveTimeoutMs = 1000;
constexpr uint64_t kIdleIntervalMs = 1000 / kFrameRate;
// Players send a keepalive within the session timeout; a silent one is dropped.
constexpr int kSessionTimeoutSeconds = 60;
constexpr uint64_t kReceiveTimeoutMs = (kSessionTimeoutSeconds + 5) * 1000;
constexpr size_t kMaxRequestBytes = 16384;
constexpr int kSendBufferBytes = 8 << 20;  // a few frames' worth of packets per player
constexpr uint64_t kUrlRefreshMs = 5000;
constexpr uint64_t kSenderReportIntervalMs = 1000;
constexpr uint64_t kSeconds1601To1900 = 9435484800ull;

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

// The value of header `name` in an RTSP request, matched case-insensitively.
std::string HeaderValue(const std::string& request, const char* name) {
    std::string prefix = Lower(name) + ":";
    for (size_t line = request.find("\r\n"); line != std::string::npos;) {
        line += 2;
        size_t end = request.find("\r\n", line);
        if (end == std::string::npos || end == line) {
            break;
        }
        if (Lower(request.substr(line, prefix.size())) == prefix) {
            return Trim(request.substr(line + prefix.size(), end - line - prefix.size()));
        }
        line = end;
    }
    return {};
}

std::string SessionId(const std::string& request) {
    std::string value = HeaderValue(request, "Session");
    return Trim(value.substr(0, value.find(';')));
}

std::string Hex(uint64_t value, int digits) {
    char text[17];
    snprintf(text, sizeof(text), "%0*llX", digits, static_cast<unsigned long long>(value));
    return text;
}

std::string AddressText(const sockaddr_in& address) {
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

SOCKET BindUdp(in_addr address, uint16_t preferredPort, uint16_t& port) {
    for (uint16_t candidate : {preferredPort, uint16_t(0)}) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            return s;
        }
        sockaddr_in bound = {};
        bound.sin_family = AF_INET;
        bound.sin_addr = address;
        bound.sin_port = htons(candidate);
        int length = sizeof(bound);
        if (bind(s, reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) == 0 &&
            getsockname(s, reinterpret_cast<sockaddr*>(&bound), &length) == 0) {
            port = ntohs(bound.sin_port);
            return s;
        }
        closesocket(s);
    }
    return INVALID_SOCKET;
}

// IPv4 addresses other devices can reach this PC on. Prefers adapters with a default
// gateway, which leaves out virtual switches (Hyper-V, WSL) when there's a real network.
std::vector<std::wstring> LanAddresses() {
    ULONG size = 16384;
    std::vector<uint8_t> buffer;
    ULONG result;
    do {
        buffer.resize(size);
        result = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    } while (result == ERROR_BUFFER_OVERFLOW);
    std::vector<std::wstring> withGateway, others;
    if (result != NO_ERROR) {
        return others;
    }
    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            auto* address = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            wchar_t text[INET_ADDRSTRLEN];
            if (InetNtopW(AF_INET, &address->sin_addr, text, INET_ADDRSTRLEN)) {
                (adapter->FirstGatewayAddress ? withGateway : others).push_back(text);
            }
        }
    }
    return withGateway.empty() ? others : withGateway;
}

// RTP timestamps run at 90 kHz for video. RTP keeps the low 32 bits; the encoder gets the
// whole value, as sample times must not wrap.
uint64_t RtpClock64() {
    static const LONGLONG freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return uint64_t(now.QuadPart / freq * 90000 + now.QuadPart % freq * 90000 / freq);
}

uint32_t RtpClock() { return uint32_t(RtpClock64()); }

// RTCP sender report plus the SDES CNAME every compound packet needs (RFC 3550 6.4.1, 6.5).
// It ties RTP timestamps to wall-clock time, so players time frames by when they were
// captured rather than guessing from arrival, and don't buffer extra to be safe.
std::string SenderReport(uint32_t ssrc, uint32_t packets, uint32_t octets) {
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    uint32_t rtpNow = RtpClock();
    uint64_t ticks = uint64_t(now.dwHighDateTime) << 32 | now.dwLowDateTime;  // 100 ns
    uint32_t ntpSeconds = uint32_t(ticks / 10000000 - kSeconds1601To1900);
    uint32_t ntpFraction = uint32_t((ticks % 10000000 << 32) / 10000000);

    std::string report;
    auto put32 = [&](uint32_t v) {
        for (int shift = 24; shift >= 0; shift -= 8) report += char(v >> shift);
    };
    put32(0x80C80006);  // V=2, no report blocks, SR, 7 words
    put32(ssrc);
    put32(ntpSeconds);
    put32(ntpFraction);
    put32(rtpNow);
    put32(packets);
    put32(octets);
    put32(0x81CA0004);  // V=2, one chunk, SDES, 5 words
    put32(ssrc);
    report +=
        "\x01\x07"
        "Gazeora";           // CNAME
    report.append(2, '\0');  // end of items, padded to a word
    return report;
}

}  // namespace

bool FrameStreamServer::Start(uint16_t port, bool lan, std::wstring& error) {
    if (Running()) {
        return true;
    }
    WSADATA wsa;
    if (int result = WSAStartup(MAKEWORD(2, 2), &wsa)) {
        error = L"WSAStartup failed (error " + std::to_wstring(result) + L")";
        return false;
    }
    m_wsaStarted = true;
    m_lan = lan;
    in_addr bindAddress;
    bindAddress.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);

    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        error = L"socket failed (error " + std::to_wstring(WSAGetLastError()) + L")";
        Stop();
        return false;
    }
    // Don't let another program take the port over from under us.
    BOOL exclusive = TRUE;
    setsockopt(m_listen, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive),
               sizeof(exclusive));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr = bindAddress;
    address.sin_port = htons(port);
    int length = sizeof(address);
    if (bind(m_listen, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(m_listen, SOMAXCONN) == SOCKET_ERROR ||
        getsockname(m_listen, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR) {
        int code = WSAGetLastError();
        error = code == WSAEADDRINUSE ? L"port " + std::to_wstring(port) + L" is already in use"
                                      : L"cannot listen on port " + std::to_wstring(port) +
                                            L" (error " + std::to_wstring(code) + L")";
        Stop();
        return false;
    }
    uint16_t rtspPort = ntohs(address.sin_port);

    // RTP goes out on the same port number over UDP when it's free, RTCP on the next one.
    m_rtp = BindUdp(bindAddress, rtspPort, m_rtpPort);
    m_rtcp = m_rtp == INVALID_SOCKET ? INVALID_SOCKET
                                     : BindUdp(bindAddress, uint16_t(m_rtpPort + 1), m_rtcpPort);
    if (m_rtcp == INVALID_SOCKET) {
        error = L"cannot open the UDP ports (error " + std::to_wstring(WSAGetLastError()) + L")";
        Stop();
        return false;
    }
    int sendBuffer = kSendBufferBytes;
    setsockopt(m_rtp, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBuffer),
               sizeof(sendBuffer));
    // Receiver reports are read between frames, without blocking. And without this, a
    // sender report to a player that has gone makes the next read fail with WSAECONNRESET.
    u_long nonBlocking = 1;
    ioctlsocket(m_rtcp, FIONBIO, &nonBlocking);
    BOOL reportResets = FALSE;
    DWORD returned = 0;
    WSAIoctl(m_rtcp, SIO_UDP_CONNRESET, &reportResets, sizeof(reportResets), nullptr, 0, &returned,
             nullptr, nullptr);

    m_ssrc = std::random_device()();
    m_h265Available = VideoEncoder::Available(VideoCodec::H265);
    m_stopping = false;
    m_frameReady = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_port = rtspPort;
    Log("listening on port %u, RTP from UDP %u, RTCP %u", rtspPort, m_rtpPort, m_rtcpPort);
    m_encodeThread = std::thread([this] { EncodeLoop(); });
    m_acceptThread = std::thread([this] { AcceptLoop(); });
    return true;
}

void FrameStreamServer::Stop() {
    m_stopping = true;
    if (m_frameReady) {
        SetEvent(m_frameReady);
    }
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    if (m_encodeThread.joinable()) {
        m_encodeThread.join();
    }
    ReapConnections(true);
    for (SOCKET* s : {&m_listen, &m_rtp, &m_rtcp}) {
        if (*s != INVALID_SOCKET) {
            closesocket(*s);
            *s = INVALID_SOCKET;
        }
    }
    if (m_frameReady) {
        CloseHandle(m_frameReady);
        m_frameReady = nullptr;
    }
    if (m_wsaStarted) {
        WSACleanup();
        m_wsaStarted = false;
    }
    m_port = 0;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_sessions.clear();
        m_viewers = 0;
    }
    std::lock_guard<std::mutex> lock(m_urlsMutex);
    m_urls.clear();
}

std::vector<std::wstring> FrameStreamServer::Urls() {
    uint16_t port = m_port;
    if (!port) {
        return {};
    }
    std::lock_guard<std::mutex> lock(m_urlsMutex);
    uint64_t now = GetTickCount64();
    if (m_urls.empty() || now - m_urlsAtMs > kUrlRefreshMs) {
        std::vector<std::wstring> addresses;
        if (m_lan) {
            addresses = LanAddresses();
        }
        if (addresses.empty()) {
            addresses.push_back(L"127.0.0.1");
        }
        m_urls.clear();
        for (const auto& address : addresses) {
            m_urls.push_back(L"rtsp://" + address + L":" + std::to_wstring(port) + L"/");
        }
        m_urlsAtMs = now;
    }
    return m_urls;
}

void FrameStreamServer::SetVrLayout(const VrLayout& vr) {
    std::lock_guard<std::mutex> lock(m_vrMutex);
    m_vr = vr;
}

VrLayout FrameStreamServer::GetVrLayout() {
    std::lock_guard<std::mutex> lock(m_vrMutex);
    return m_vr;
}

std::wstring FrameStreamServer::LastError() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void FrameStreamServer::PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>& bgra) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_raw.swap(bgra);
    m_rawClock = RtpClock64();
    m_rawWidth = width;
    m_rawHeight = height;
    m_rawPending = true;
    if (m_frameReady) {
        SetEvent(m_frameReady);
    }
}

void FrameStreamServer::SetIdleFrame(uint32_t width, uint32_t height, std::vector<uint8_t> bgra) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_idle = std::move(bgra);
    m_idleWidth = width;
    m_idleHeight = height;
    m_idlePending = true;
    if (m_frameReady) {
        SetEvent(m_frameReady);
    }
}

void FrameStreamServer::SetLogFile(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_log) {
        fclose(m_log);
        m_log = nullptr;
    }
    m_logBytes = 0;
    if (!path.empty()) {
        m_log = _wfopen(path.c_str(), L"w");
    }
}

void FrameStreamServer::Log(const char* format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (!m_log || m_logBytes > kMaxLogBytes) {
        return;
    }
    int written = fprintf(m_log, "%02d:%02d:%02d.%03d  %s\n", time.wHour, time.wMinute,
                          time.wSecond, time.wMilliseconds, text);
    fflush(m_log);
    m_logBytes += std::max(written, 0);
}

bool FrameStreamServer::Flush(Connection& connection) {
    size_t sent = 0;
    bool ok = true;
    while (sent < connection.out.size()) {
        int n = send(connection.socket, connection.out.data() + sent,
                     int(std::min<size_t>(connection.out.size() - sent, 1 << 20)), 0);
        if (n > 0) {
            sent += size_t(n);
        } else {
            ok = n < 0 && WSAGetLastError() == WSAEWOULDBLOCK;  // full for now
            break;
        }
    }
    connection.out.erase(0, sent);
    return ok;
}

bool FrameStreamServer::Send(Connection& connection, const std::string& data) {
    std::lock_guard<std::mutex> lock(connection.outMutex);
    connection.out += data;
    return Flush(connection);
}

void FrameStreamServer::AcceptLoop() {
    while (!m_stopping) {
        // Poll so Stop doesn't have to close the socket out from under accept().
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listen, &readable);
        timeval timeout = {0, 250 * 1000};
        if (select(0, &readable, nullptr, nullptr, &timeout) != 1) {
            ReapConnections(false);
            continue;
        }
        sockaddr_in peer = {};
        int peerLength = sizeof(peer);
        SOCKET client = accept(m_listen, reinterpret_cast<sockaddr*>(&peer), &peerLength);
        if (client == INVALID_SOCKET) {
            continue;
        }
        BOOL noDelay = TRUE;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&noDelay),
                   sizeof(noDelay));
        int sendBuffer = kTcpSendBufferBytes;
        setsockopt(client, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBuffer),
                   sizeof(sendBuffer));
        u_long nonBlocking = 1;
        ioctlsocket(client, FIONBIO, &nonBlocking);

        auto connection = std::make_unique<Connection>();
        connection->socket = client;
        connection->peer = AddressText(peer);
        Log("%s connected", connection->peer.c_str());
        Connection* raw = connection.get();
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections.push_back(std::move(connection));
        raw->thread = std::thread([this, raw] { Serve(*raw); });
    }
}

void FrameStreamServer::ReapConnections(bool all) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        Connection& connection = **it;
        // With `all`, Stop has set m_stopping, which the connection notices within a poll.
        if (all || connection.done) {
            if (connection.thread.joinable()) {
                connection.thread.join();
            }
            closesocket(connection.socket);
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

void FrameStreamServer::UpdateViewers() {
    m_viewers = int(std::count_if(m_sessions.begin(), m_sessions.end(),
                                  [](const auto& entry) { return entry.second.playing; }));
}

void FrameStreamServer::Serve(Connection& connection) {
    sockaddr_in peer = {}, local = {};
    int length = sizeof(peer);
    getpeername(connection.socket, reinterpret_cast<sockaddr*>(&peer), &length);
    length = sizeof(local);
    getsockname(connection.socket, reinterpret_cast<sockaddr*>(&local), &length);

    std::string buffer;
    char chunk[4096];
    uint64_t connectedMs = GetTickCount64();
    uint64_t lastRequestMs = connectedMs;
    const char* reason = "the server stopped";
    const uint32_t generation = m_codecGeneration;
    while (!m_stopping) {
        // A player's decoder is set up for the codec it was described; it can't follow a
        // change. Closing the connection makes it stop, so it can be opened again.
        if (m_codecGeneration != generation) {
            reason = "the video codec changed";
            break;
        }
        if (!buffer.empty() && buffer[0] == '$') {
            // Interleaved data from the player: its RTCP receiver reports. Nothing is lost
            // on TCP, so there's no loss to read from them.
            if (buffer.size() >= 4) {
                size_t total = 4 + (uint8_t(buffer[2]) << 8 | uint8_t(buffer[3]));
                if (buffer.size() >= total) {
                    buffer.erase(0, total);
                    continue;
                }
            }
        } else {
            size_t headEnd = buffer.find("\r\n\r\n");
            size_t total = std::string::npos;
            if (headEnd != std::string::npos) {
                std::string head = buffer.substr(0, headEnd + 2);
                total = headEnd + 4 +
                        strtoul(HeaderValue(head, "Content-Length").c_str(), nullptr, 10);
            }
            if (total != std::string::npos && buffer.size() >= total) {
                std::string request = buffer.substr(0, total);
                buffer.erase(0, total);
                if (!Respond(connection, request, peer, local)) {
                    reason = "it sent something other than RTSP, or the reply failed";
                    break;
                }
                continue;
            }
            if (buffer.size() > kMaxRequestBytes ||
                (total != std::string::npos && total > kMaxRequestBytes)) {
                reason = "its request was too large";
                break;
            }
        }
        // Poll, as a blocking recv can't be interrupted from Stop on Windows. While RTP
        // waits to go out on this connection, wait for room for it too, and look again soon:
        // the encoder only adds to it.
        bool pending;
        {
            std::lock_guard<std::mutex> lock(connection.outMutex);
            pending = !connection.out.empty();
        }
        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_SET(connection.socket, &readable);
        FD_ZERO(&writable);
        FD_SET(connection.socket, &writable);
        timeval timeout = {0, (connection.interleaved ? 5 : 250) * 1000};
        int ready = select(0, &readable, pending ? &writable : nullptr, nullptr, &timeout);
        if (ready < 0) {
            reason = "the connection failed";
            break;
        }
        if (pending && FD_ISSET(connection.socket, &writable)) {
            std::lock_guard<std::mutex> lock(connection.outMutex);
            if (!Flush(connection)) {
                reason = "sending to it failed";
                break;
            }
        }
        if (!FD_ISSET(connection.socket, &readable)) {
            if (GetTickCount64() - lastRequestMs > kReceiveTimeoutMs) {
                reason = "it was silent past the session timeout";
                break;
            }
            continue;
        }
        int got = recv(connection.socket, chunk, sizeof(chunk), 0);
        if (got == 0) {
            reason = "it closed the connection";
            break;
        }
        if (got < 0) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                continue;
            }
            reason = WSAGetLastError() == WSAECONNRESET ? "it reset the connection"
                                                        : "the connection broke";
            break;
        }
        buffer.append(chunk, size_t(got));
        lastRequestMs = GetTickCount64();
    }

    // The player is gone: stop sending to it.
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto it = m_sessions.begin(); it != m_sessions.end();) {
            it = it->second.owner == &connection ? m_sessions.erase(it) : std::next(it);
        }
        UpdateViewers();
    }
    Log("%s disconnected after %llu s: %s", connection.peer.c_str(),
        (unsigned long long)((GetTickCount64() - connectedMs) / 1000), reason);
    // The socket is closed by ReapConnections, after this thread is joined.
    shutdown(connection.socket, SD_SEND);
    connection.done = true;
}

bool FrameStreamServer::Respond(Connection& connection, const std::string& request,
                                const sockaddr_in& peer, const sockaddr_in& local) {
    std::string line = request.substr(0, request.find("\r\n"));
    size_t methodEnd = line.find(' ');
    size_t urlEnd = methodEnd == std::string::npos ? methodEnd : line.find(' ', methodEnd + 1);
    if (urlEnd == std::string::npos) {
        return false;  // not RTSP
    }
    std::string method = line.substr(0, methodEnd);
    std::string url = line.substr(methodEnd + 1, urlEnd - methodEnd - 1);

    auto reply = [&](const char* status, const std::string& headers = {},
                     const std::string& body = {}) {
        std::string response = std::string("RTSP/1.0 ") + status +
                               "\r\nCSeq: " + HeaderValue(request, "CSeq") +
                               "\r\nServer: Gazeora\r\n" + headers;
        if (!body.empty()) {
            response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        }
        return Send(connection, response + "\r\n" + body);
    };

    if (method == "OPTIONS") {
        return reply("200 OK",
                     "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, "
                     "SET_PARAMETER\r\n");
    }
    if (method == "DESCRIBE") {
        char ip[INET_ADDRSTRLEN] = "0.0.0.0";
        inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
        std::string payloadType = std::to_string(kRtpVideoPayloadType);
        VideoCodec codec = Codec();
        std::string fmtp;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            fmtp = m_fmtp[int(codec)];
        }
        std::string sdp = "v=0\r\no=- " + std::to_string(m_ssrc) + " 1 IN IP4 " + ip +
                          "\r\ns=Gazeora\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n"
                          "m=video 0 RTP/AVP " +
                          payloadType + "\r\na=rtpmap:" + payloadType +
                          (codec == VideoCodec::H265 ? " H265/90000\r\n" : " H264/90000\r\n") +
                          (fmtp.empty() ? "" : "a=fmtp:" + payloadType + " " + fmtp + "\r\n") +
                          "a=control:track1\r\n";
        std::string base = !url.empty() && url.back() == '/' ? url : url + "/";
        return reply("200 OK", "Content-Base: " + base + "\r\nContent-Type: application/sdp\r\n",
                     sdp);
    }
    if (method == "SETUP") {
        // UDP unicast, or interleaved in this connection. No multicast: players fall back
        // to unicast when it's refused.
        std::string transport = Lower(HeaderValue(request, "Transport"));
        size_t portAt = transport.find("client_port=");
        size_t interleavedAt = transport.find("interleaved");
        bool tcp = interleavedAt != std::string::npos;
        if (transport.find("multicast") != std::string::npos ||
            (!tcp && (transport.find("/tcp") != std::string::npos || portAt == std::string::npos))) {
            return reply("461 Unsupported Transport");
        }
        unsigned long rtpPort = 0, rtcpPort = 0, channel = 0;
        if (tcp) {
            if (transport.compare(interleavedAt, 12, "interleaved=") == 0) {
                channel = strtoul(transport.c_str() + interleavedAt + 12, nullptr, 10);
            }
            if (channel > 254) {
                return reply("461 Unsupported Transport");
            }
        } else {
            char* rangeEnd = nullptr;
            rtpPort = strtoul(transport.c_str() + portAt + 12, &rangeEnd, 10);
            rtcpPort = *rangeEnd == '-' ? strtoul(rangeEnd + 1, nullptr, 10) : 0;
            if (rtpPort == 0 || rtpPort > 65534) {
                return reply("461 Unsupported Transport");
            }
            if (rtcpPort == 0 || rtcpPort > 65535) {
                rtcpPort = rtpPort + 1;
            }
        }
        std::string id = SessionId(request);
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(id);
        if (it == m_sessions.end() || it->second.owner != &connection) {
            static std::mt19937_64 random{std::random_device()()};
            id = Hex(random(), 16);
            it = m_sessions.emplace(id, Session{&connection}).first;
        }
        Session& session = it->second;
        session.tcp = tcp;
        std::string replyTransport;
        if (tcp) {
            session.channel = uint8_t(channel);
            connection.interleaved = true;
            replyTransport = "RTP/AVP/TCP;unicast;interleaved=" + std::to_string(channel) + "-" +
                             std::to_string(channel + 1);
            Log("%s set up RTP over TCP, interleaved on channel %lu", connection.peer.c_str(),
                channel);
        } else {
            session.rtp = peer;
            session.rtp.sin_port = htons(uint16_t(rtpPort));
            session.rtcp = peer;
            session.rtcp.sin_port = htons(uint16_t(rtcpPort));
            replyTransport = "RTP/AVP;unicast;client_port=" + std::to_string(rtpPort) + "-" +
                             std::to_string(rtcpPort) + ";server_port=" +
                             std::to_string(m_rtpPort) + "-" + std::to_string(m_rtcpPort);
            Log("%s set up RTP over UDP to ports %lu-%lu", connection.peer.c_str(), rtpPort,
                rtcpPort);
        }
        return reply("200 OK", "Transport: " + replyTransport + ";ssrc=" + Hex(m_ssrc, 8) +
                                   "\r\nSession: " + id + ";timeout=" +
                                   std::to_string(kSessionTimeoutSeconds) + "\r\n");
    }
    if (method == "PLAY" || method == "PAUSE" || method == "TEARDOWN") {
        std::string id = SessionId(request);
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(id);
        if (it == m_sessions.end() || it->second.owner != &connection) {
            return reply("454 Session Not Found");
        }
        Log("%s: %s", connection.peer.c_str(), method.c_str());
        if (method == "TEARDOWN") {
            m_sessions.erase(it);
        } else {
            Session& session = it->second;
            session.playing = method == "PLAY";
            session.needsReport = session.playing;
            if (session.playing) {
                m_keyFrameWanted = true;  // so it doesn't wait up to a second for a picture
                session.waitingForKeyFrame = false;
                session.framesSent = session.framesSkipped = 0;
                session.countedSinceMs = session.lastHeardMs = GetTickCount64();
                session.quietLogged = false;
            }
        }
        UpdateViewers();
        return reply("200 OK",
                     "Session: " + id + "\r\n" + (method == "PLAY" ? "Range: npt=0.000-\r\n" : ""));
    }
    if (method == "GET_PARAMETER" || method == "SET_PARAMETER") {
        return reply("200 OK");  // keepalive
    }
    return reply("501 Not Implemented");
}

void FrameStreamServer::SendPackets(const std::vector<std::string>& packets, bool keyFrame) {
    uint64_t now = GetTickCount64();
    bool reportDue = now - m_lastReportMs >= kSenderReportIntervalMs;
    for (const std::string& packet : packets) {
        ++m_packetsSent;
        m_octetsSent += uint32_t(packet.size() - 12);  // payload only
    }
    std::string report = SenderReport(m_ssrc, m_packetsSent, m_octetsSent);
    std::vector<sockaddr_in> targets, reportTargets;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& entry : m_sessions) {
            Session& session = entry.second;
            if (!session.playing) {
                continue;
            }
            bool reportNow = reportDue || session.needsReport;
            session.needsReport = false;
            if (!session.tcp) {
                targets.push_back(session.rtp);
                if (reportNow) {
                    reportTargets.push_back(session.rtcp);
                }
                continue;
            }
            // Interleaved: the whole frame is queued at once, TCP paces it. A frame that
            // doesn't fit the backlog is skipped, and so is every frame after it up to the
            // next key frame, as the player couldn't decode them.
            Connection& connection = *session.owner;
            std::lock_guard<std::mutex> out(connection.outMutex);
            bool full = connection.out.size() > kTcpBacklogBytes;
            if (full || (session.waitingForKeyFrame && !keyFrame)) {
                if (!full && session.waitingForKeyFrame) {
                    m_keyFrameWanted = true;
                }
                session.waitingForKeyFrame = true;
                ++session.framesSkipped;
            } else {
                session.waitingForKeyFrame = false;
                ++session.framesSent;
                auto append = [&](uint8_t channel, const std::string& data) {
                    char header[4] = {'$', char(channel), char(data.size() >> 8),
                                      char(data.size())};
                    connection.out.append(header, 4);
                    connection.out += data;
                };
                for (const std::string& packet : packets) {
                    append(session.channel, packet);
                }
                if (reportNow) {
                    append(uint8_t(session.channel + 1), report);
                }
                Flush(connection);  // a broken connection is noticed by its own thread
            }
            // Skipped frames count like the loss a UDP player reports, so the bitrate comes
            // down for a connection that can't keep up.
            if (now - session.countedSinceMs >= 1000) {
                uint32_t frames = session.framesSent + session.framesSkipped;
                if (session.framesSkipped > 0) {
                    m_lossReports[uint64_t(uintptr_t(&connection))] = {
                        float(session.framesSkipped) / float(frames), now};
                    Log("%s skipped %u of %u frames: the connection can't keep up",
                        connection.peer.c_str(), session.framesSkipped, frames);
                }
                session.framesSent = session.framesSkipped = 0;
                session.countedSinceMs = now;
            }
        }
    }
    size_t perBurst = std::max(kMinPacketsPerBurst, (packets.size() + kSpreadMs - 1) / kSpreadMs);
    for (size_t first = 0; first < packets.size(); first += perBurst) {
        if (first > 0) {
            LARGE_INTEGER due;
            due.QuadPart = -kBurstIntervalUs * 10;  // relative, in 100 ns units
            if (m_paceTimer && SetWaitableTimer(m_paceTimer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(m_paceTimer, INFINITE);
            } else {
                Sleep(1);
            }
        }
        size_t end = std::min(first + perBurst, packets.size());
        for (const sockaddr_in& target : targets) {
            for (size_t i = first; i < end; ++i) {
                // A lost or refused packet only costs that frame.
                sendto(m_rtp, packets[i].data(), int(packets[i].size()), 0,
                       reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            }
        }
    }
    for (const sockaddr_in& target : reportTargets) {
        sendto(m_rtcp, report.data(), int(report.size()), 0,
               reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    }
    if (reportDue) {
        m_lastReportMs = now;
    }
}

void FrameStreamServer::ReadReceiverReports(uint64_t now) {
    uint8_t packet[1500];
    sockaddr_in from;
    int fromLength = sizeof(from);
    int got;
    while ((got = recvfrom(m_rtcp, reinterpret_cast<char*>(packet), sizeof(packet), 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLength)) > 0) {
        uint64_t player = uint64_t(from.sin_addr.s_addr) << 16 | from.sin_port;
        // A compound packet; receiver reports (type 201) carry one block per source.
        for (int at = 0; at + 8 <= got;) {
            int length = ((packet[at + 2] << 8 | packet[at + 3]) + 1) * 4;
            int blocks = packet[at] & 0x1F;
            if (packet[at + 1] == 201) {
                for (int block = 0; block < blocks && at + 8 + (block + 1) * 24 <= got; ++block) {
                    const uint8_t* p = packet + at + 8 + block * 24;
                    uint32_t source = uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
                    if (source == m_ssrc) {
                        // Cumulative loss is signed 24-bit. VLC's first report has it at -1
                        // (one packet more than expected) and a fraction of 98%, which would
                        // send the bitrate to the bottom of the ladder; negative means no loss.
                        int32_t lost = int32_t(uint32_t(p[5]) << 24 | p[6] << 16 | p[7] << 8) >> 8;
                        float fraction = lost < 0 ? 0.0f : p[4] / 256.0f;
                        m_lossReports[player] = {fraction, now};
                        uint32_t jitter = uint32_t(p[12]) << 24 | p[13] << 16 | p[14] << 8 | p[15];
                        Log("%s reports %.1f%% lost lately, %d in all, jitter %u ms",
                            AddressText(from).c_str(), p[4] * 100.0 / 256, lost, jitter / 90);
                        HeardFrom(from, now);
                    }
                }
            }
            at += length;
        }
        fromLength = sizeof(from);
    }

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& entry : m_sessions) {
        Session& session = entry.second;
        if (session.playing && !session.tcp && !session.quietLogged &&
            now - session.lastHeardMs > kQuietPlayerMs) {
            session.quietLogged = true;
            Log("%s has sent no receiver report for %llu s: it may have stopped receiving",
                session.owner->peer.c_str(),
                (unsigned long long)((now - session.lastHeardMs) / 1000));
        }
    }
}

void FrameStreamServer::HeardFrom(const sockaddr_in& from, uint64_t now) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& entry : m_sessions) {
        Session& session = entry.second;
        if (!session.tcp && session.rtcp.sin_addr.s_addr == from.sin_addr.s_addr) {
            if (session.quietLogged) {
                Log("%s is reporting again", session.owner->peer.c_str());
            }
            session.lastHeardMs = now;
            session.quietLogged = false;
        }
    }
}

uint32_t FrameStreamServer::BitrateAt(int level) const {
    uint32_t bitrate = kBitrates[level];
    return Codec() == VideoCodec::H265 ? bitrate / 100 * kH265BitratePercent : bitrate;
}

uint32_t FrameStreamServer::Bitrate() const { return BitrateAt(m_qualityLevel); }

uint32_t FrameStreamServer::FullBitrate() const { return BitrateAt(0); }

void FrameStreamServer::SetCodec(VideoCodec codec) {
    VideoCodec before = Codec();
    m_requestedCodec = codec;
    if (Codec() != before) {
        ++m_codecGeneration;
        Log("codec now %s; players were disconnected", CodecName(Codec()));
    }
}

VideoCodec FrameStreamServer::Codec() const {
    return m_requestedCodec == VideoCodec::H265 && m_h265Available ? VideoCodec::H265
                                                                   : VideoCodec::H264;
}

bool FrameStreamServer::CodecAvailable(VideoCodec codec) const {
    return codec == VideoCodec::H264 || m_h265Available;
}

std::wstring FrameStreamServer::EncoderName() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_encoderName;
}

void FrameStreamServer::AdaptQuality(uint64_t now) {
    float worst = 0;
    for (auto it = m_lossReports.begin(); it != m_lossReports.end();) {
        if (now - it->second.atMs > kLossWindowMs) {
            it = m_lossReports.erase(it);
        } else {
            worst = std::max(worst, it->second.fraction);
            ++it;
        }
    }
    m_packetLoss = worst;
    int level = m_qualityLevel;
    if (m_viewers == 0) {
        level = 0;  // the next player starts at full quality
    } else if (worst >= kLossToBackOff) {
        m_lastLossMs = now;
        if (now - m_lastQualityChangeMs >= kBackOffEveryMs && level < kLowestLevel) {
            ++level;
            m_lastQualityChangeMs = now;
        }
    } else if (now - m_lastLossMs >= kRecoverAfterMs &&
               now - m_lastQualityChangeMs >= kRecoverEveryMs && level > 0) {
        --level;
        m_lastQualityChangeMs = now;
    }
    if (level != m_qualityLevel && m_viewers > 0) {
        Log("bitrate now %.1f Mbit/s; worst loss reported lately %.1f%%", BitrateAt(level) / 1e6,
            worst * 100.0);
    }
    m_qualityLevel = level;
}

void FrameStreamServer::EncodeLoop() {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool mediaFoundation = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    if (!mediaFoundation) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError =
            L"Media Foundation is unavailable (Windows N without the Media Feature Pack?)";
    }
    // A 1 ms timer for this process, so the idle frame keeps a steady 30 fps; the default
    // 15.6 ms one turns 33 ms waits into 47.
    timeBeginPeriod(1);
    // High resolution, so bursts can be a millisecond apart; Sleep(1) often takes 15 ms.
    m_paceTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_ALL_ACCESS);

    VideoEncoder encoder;
    int encoderLevel = 0;
    std::vector<uint8_t> pixels, idlePixels, nv12;
    uint32_t idleWidth = 0, idleHeight = 0;
    std::vector<VideoEncoder::Frame> encoded;
    std::vector<std::string> nalUnits, packets;
    std::string vps, sps, pps;  // the latest parameter sets (vps: H.265 only)
    uint16_t seq = uint16_t(std::random_device()());
    uint64_t lastLiveMs = 0;
    uint64_t nextIdleMs = 0;  // on the 90 kHz clock's millisecond scale; GetTickCount64 is too
                              // coarse for 33 ms

    while (mediaFoundation) {
        bool gotLive = false;
        uint32_t width = 0, height = 0;
        uint64_t clock = 0;
        // Until a frame arrives or the next idle frame is due.
        uint64_t clockMs = RtpClock64() / 90;
        uint64_t waitMs = nextIdleMs > clockMs ? nextIdleMs - clockMs : 0;
        WaitForSingleObject(m_frameReady, DWORD(std::min(waitMs, kIdleIntervalMs)));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) {
                break;
            }
            if (m_rawPending) {
                pixels.swap(m_raw);
                clock = m_rawClock;
                width = m_rawWidth;
                height = m_rawHeight;
                m_rawPending = false;
                gotLive = true;
            }
            if (m_idlePending) {
                idlePixels = std::move(m_idle);
                idleWidth = m_idleWidth;
                idleHeight = m_idleHeight;
                m_idlePending = false;
            }
        }

        uint64_t now = GetTickCount64();
        ReadReceiverReports(now);
        AdaptQuality(now);

        // The idle frame goes through the encoder too, like a game that shows a still picture.
        const std::vector<uint8_t>* source = nullptr;
        clockMs = RtpClock64() / 90;  // after the wait
        if (gotLive) {
            source = &pixels;
            lastLiveMs = now;
            nextIdleMs = clockMs + kIdleIntervalMs;
        } else if (!idlePixels.empty() && m_viewers > 0 && now - lastLiveMs >= kLiveTimeoutMs) {
            if (clockMs >= nextIdleMs) {
                source = &idlePixels;
                width = idleWidth;
                height = idleHeight;
                clock = RtpClock64();
                // One interval on from when this one was due, so the time spent encoding
                // doesn't add up; more than an interval behind (after a pause), from now.
                nextIdleMs = clockMs - nextIdleMs > kIdleIntervalMs ? clockMs + kIdleIntervalMs
                                                                    : nextIdleMs + kIdleIntervalMs;
            }
        } else {
            nextIdleMs = clockMs + kIdleIntervalMs;  // nothing to repeat: don't spin
        }
        if (!source || source->size() < size_t(width) * height * 4) {
            continue;
        }

        std::wstring error;
        uint32_t encodeWidth = width & ~1u, encodeHeight = height & ~1u;
        VideoCodec codec = Codec();
        if (!encoder.IsOpen() || encoder.Codec() != codec || encoder.Width() != encodeWidth ||
            encoder.Height() != encodeHeight) {
            encoderLevel = m_qualityLevel;
            if (!encoder.Open(codec, encodeWidth, encodeHeight, kFrameRate,
                              BitrateAt(encoderLevel), kKeyFrameInterval, true, error)) {
                Log("cannot open an %s encoder for %ux%u: %ls", CodecName(codec), encodeWidth,
                    encodeHeight, error.c_str());
                if (codec == VideoCodec::H265) {
                    // Listed but not working (a size it doesn't take, a driver problem): fall
                    // back to H.264, which always has the software encoder.
                    VideoCodec before = Codec();
                    m_h265Available = false;
                    if (Codec() != before) ++m_codecGeneration;
                }
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastError = error;
                continue;
            }
            Log("encoding %s %ux%u with %ls at %.1f Mbit/s", CodecName(codec), encodeWidth,
                encodeHeight, encoder.Name().c_str(), BitrateAt(encoderLevel) / 1e6);
            vps.clear();
            sps.clear();
            pps.clear();
            std::lock_guard<std::mutex> lock(m_mutex);
            m_encoderName = encoder.Name();
        }
        if (encoderLevel != m_qualityLevel) {
            encoderLevel = m_qualityLevel;
            encoder.SetBitrate(BitrateAt(encoderLevel));
        }
        if (m_keyFrameWanted.exchange(false)) {
            encoder.RequestKeyFrame();
        }

        BgraToNv12(source->data(), width, encodeWidth, encodeHeight, nv12);
        encoded.clear();
        // Sample times in 100 ns units, from the 90 kHz capture clock.
        bool ok = encoder.Encode(nv12.data(), int64_t(clock * 1000 / 9), encoded, error);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = ok ? std::wstring() : error;
        }
        if (!ok) {
            Log("encoding failed: %ls", error.c_str());
            encoder.Close();  // reopened with the next frame
            continue;
        }
        const bool hevc = encoder.Codec() == VideoCodec::H265;
        for (const VideoEncoder::Frame& frame : encoded) {
            SplitAnnexB(frame.annexB, nalUnits);
            // From the NAL units rather than the encoder's clean-point flag: an IDR (IRAP for
            // H.265) is what a player can start decoding from.
            bool hasSps = false, keyFrame = false, changed = false;
            for (const std::string& nal : nalUnits) {
                keyFrame |= IsKeyFrameNal(encoder.Codec(), nal);
                uint8_t type = hevc ? HevcNalType(nal) : NalType(nal);
                std::string* set = type == (hevc ? kHevcSps : kNalSps)   ? &sps
                                   : type == (hevc ? kHevcPps : kNalPps) ? &pps
                                   : hevc && type == kHevcVps            ? &vps
                                                                         : nullptr;
                hasSps |= set == &sps;
                if (set && *set != nal) {
                    *set = nal;
                    changed = true;
                }
            }
            if (changed) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_fmtp[int(encoder.Codec())] = hevc ? H265Fmtp(vps, sps, pps) : H264Fmtp(sps, pps);
            }
            // Every key frame carries the parameter sets, for players that join on it.
            if (keyFrame && !hasSps && !sps.empty() && !pps.empty() && (!hevc || !vps.empty())) {
                if (hevc) {
                    nalUnits.insert(nalUnits.begin(), {vps, sps, pps});
                } else {
                    nalUnits.insert(nalUnits.begin(), {sps, pps});
                }
            }
            // Back to the 90 kHz clock, rounding up so it lands on the captured value.
            uint32_t timestamp = uint32_t((uint64_t(frame.time) * 9 + 999) / 1000);
            PacketizeRtpVideo(encoder.Codec(), nalUnits, timestamp, m_ssrc, kMaxPacketSize, seq,
                              packets);
            SendPackets(packets, keyFrame);
        }
    }

    encoder.Close();
    if (mediaFoundation) {
        MFShutdown();
    }
    timeEndPeriod(1);
    if (m_paceTimer) {
        CloseHandle(m_paceTimer);
        m_paceTimer = nullptr;
    }
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
}

}  // namespace gazeora
