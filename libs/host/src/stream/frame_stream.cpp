#include "gazeora/host/stream/frame_stream.h"

#include <mfapi.h>
#include <mmsystem.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <utility>

#include "gazeora/host/stream/packet_pacer.h"
#include "gazeora/host/stream/player_identity.h"
#include "gazeora/host/stream/pointer_message.h"
#include "gazeora/host/stream/rtcp.h"
#include "gazeora/host/stream/rtp_video.h"
#include "gazeora/host/stream/rtsp_connection.h"
#include "gazeora/host/stream/rtsp_message.h"
#include "gazeora/host/stream/stream_view_info.h"

// Missing from older MinGW headers.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace gazeora {

namespace {

// H.265 gets about the same picture from 70% of the bits. The rest is saved rather than
// spent on detail: fewer packets per frame means fewer frames hit by a loss on Wi-Fi.
constexpr uint32_t kH265BitratePercent = 70;
constexpr uint32_t kFrameRate = 30;
// A key frame every second: what a lost packet smears is repaired by then, and a player
// that joins without one still starts within a second. More often costs too much picture
// quality at these bitrates.
constexpr uint32_t kKeyFrameInterval = 30;
// Interleaved players: a frame is skipped while this much is still queued for the player,
// about a fifth of a second at the default full bitrate. The socket's own buffer is kept small
// too, so what's queued stays where it can be counted.
constexpr size_t kTcpBacklogBytes = 512 * 1024;
constexpr int kTcpSendBufferBytes = 128 * 1024;
// A UDP player without a receiver report for this long is noted in the log. VLC sends one
// every few seconds, so a player that stops has most likely stopped receiving.
constexpr uint64_t kQuietPlayerMs = 12000;
// RTP packets stay under a typical 1500-byte Ethernet / Wi-Fi MTU.
constexpr size_t kMaxPacketSize = 1400;
constexpr size_t kRtpHeaderSize = 12;
// Without a live frame for this long, the idle frame is streamed, kIdleIntervalMs apart:
// the full frame rate, as players (VLC at least) drop frames as late when a stream runs much
// slower than 30 fps. A repeated still picture costs next to nothing.
constexpr uint64_t kLiveTimeoutMs = 1000;
constexpr uint64_t kIdleIntervalMs = 1000 / kFrameRate;
// How long a DESCRIBE waits for the encoder's first parameter sets. Players that can't take
// them in-band (ExoPlayer) need them in the SDP.
constexpr auto kParameterSetsWait = std::chrono::seconds(2);
// Players send a keepalive within the session timeout; a silent one is dropped.
constexpr int kSessionTimeoutSeconds = 60;
constexpr uint64_t kReceiveTimeoutMs = (kSessionTimeoutSeconds + 5) * 1000;
constexpr int kSendBufferBytes = 8 << 20;  // a few frames' worth of packets per player
constexpr uint64_t kUrlRefreshMs = 5000;
constexpr uint64_t kSenderReportIntervalMs = 1000;

std::string Hex(uint64_t value, int digits) {
    char text[17];
    snprintf(text, sizeof(text), "%0*llX", digits, static_cast<unsigned long long>(value));
    return text;
}

// A key for the bitrate controller: a UDP player by its RTCP address, an interleaved one by
// its connection.
uint64_t ReportSource(const sockaddr_in& from) {
    return uint64_t(from.sin_addr.s_addr) << 16 | from.sin_port;
}

// COM, Media Foundation and a 1 ms system timer for the encode thread's lifetime.
class EncodeThreadRuntime {
public:
    EncodeThreadRuntime()
        : m_com(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
          m_mediaFoundation(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        // A 1 ms timer for this process, so the idle frame keeps a steady 30 fps; the default
        // 15.6 ms one turns 33 ms waits into 47.
        timeBeginPeriod(1);
    }
    EncodeThreadRuntime(const EncodeThreadRuntime&) = delete;
    EncodeThreadRuntime& operator=(const EncodeThreadRuntime&) = delete;
    ~EncodeThreadRuntime() {
        timeEndPeriod(1);
        if (m_mediaFoundation) {
            MFShutdown();
        }
        if (SUCCEEDED(m_com)) {
            CoUninitialize();
        }
    }

    bool MediaFoundation() const { return m_mediaFoundation; }

private:
    HRESULT m_com;
    bool m_mediaFoundation;
};

// Frames streamed per second, over the last two (one wobbles by a frame), for the overlay.
class FrameRateCounter {
public:
    void Add(uint64_t now) { m_times.push_back(now); }
    float PerSecond(uint64_t now) {
        while (!m_times.empty() && now - m_times.front() >= 2000) {
            m_times.pop_front();
        }
        return m_times.size() / 2.0f;
    }

private:
    std::deque<uint64_t> m_times;
};

}  // namespace

FrameStreamServer::FrameStreamServer(VideoPipeline::EncoderFactory encoderFactory)
    : m_encoderFactory(std::move(encoderFactory)) {}

FrameStreamServer::~FrameStreamServer() {
    Stop();
    SetLogFile({});
}

bool FrameStreamServer::Start(uint16_t port, bool lan, std::wstring& error) {
    if (Running()) {
        return true;
    }
    if (!m_winsock.Start(error)) {
        return false;
    }
    m_lan = lan;
    in_addr bindAddress;
    bindAddress.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);

    const StreamProtocol protocol = m_protocol;
    m_runningProtocol = protocol;
    m_output = CreateFrameOutput(protocol, m_log);
    if (m_output) {
        m_output->SetPointerSink(&m_pointerGate);
        if (!m_output->Start(bindAddress, port, error)) {
            Stop();
            return false;
        }
        m_port = m_output->Port();
    } else if (!StartRtsp(bindAddress, port, error)) {
        Stop();
        return false;
    }

    m_ssrc = std::random_device()();
    m_h265Available = VideoEncoder::Available(VideoCodec::H265);
    m_stopping = false;
    std::wstring discoveryError;
    if (!m_discovery.Start(bindAddress, m_discoveryPort, [this] { return Describe(); },
                           discoveryError)) {
        m_log.Write("the Android app can't search for this PC: %ls", discoveryError.c_str());
    }
    std::wstring pointerError;
    if (!m_pointerReceiver.Start(bindAddress, kPointerPort, pointerError)) {
        m_log.Write("touches from the Android app won't reach this PC: %ls",
                    pointerError.c_str());
    }
    m_log.Write("streaming over %s", ProtocolName(protocol));
    m_running = true;
    // The idle frame set before a restart (a protocol change) still stands.
    m_mailbox.RepostIdle();
    m_encodeThread = std::thread([this] { EncodeLoop(); });
    if (!m_output) {
        m_acceptThread = std::thread([this] { AcceptLoop(); });
    }
    return true;
}

bool FrameStreamServer::StartRtsp(in_addr bindAddress, uint16_t port, std::wstring& error) {
    m_listen.Reset(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!m_listen) {
        error = L"socket failed (error " + std::to_wstring(WSAGetLastError()) + L")";
        return false;
    }
    // Don't let another program take the port over from under us.
    BOOL exclusive = TRUE;
    setsockopt(m_listen.Get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<char*>(&exclusive), sizeof(exclusive));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr = bindAddress;
    address.sin_port = htons(port);
    int length = sizeof(address);
    if (bind(m_listen.Get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
            SOCKET_ERROR ||
        listen(m_listen.Get(), SOMAXCONN) == SOCKET_ERROR ||
        getsockname(m_listen.Get(), reinterpret_cast<sockaddr*>(&address), &length) ==
            SOCKET_ERROR) {
        int code = WSAGetLastError();
        error = code == WSAEADDRINUSE ? L"port " + std::to_wstring(port) + L" is already in use"
                                      : L"cannot listen on port " + std::to_wstring(port) +
                                            L" (error " + std::to_wstring(code) + L")";
        return false;
    }
    uint16_t rtspPort = ntohs(address.sin_port);

    // RTP goes out on the same port number over UDP when it's free, RTCP on the next one.
    m_rtp = net::BindUdp(bindAddress, rtspPort, m_rtpPort);
    if (m_rtp) {
        m_rtcp = net::BindUdp(bindAddress, uint16_t(m_rtpPort + 1), m_rtcpPort);
    }
    if (!m_rtcp) {
        error = L"cannot open the UDP ports (error " + std::to_wstring(WSAGetLastError()) + L")";
        return false;
    }
    m_rtp.SetSendBuffer(kSendBufferBytes);
    // Receiver reports are read between frames, without blocking. And without this, a
    // sender report to a player that has gone makes the next read fail with WSAECONNRESET.
    m_rtcp.SetNonBlocking();
    BOOL reportResets = FALSE;
    DWORD returned = 0;
    WSAIoctl(m_rtcp.Get(), SIO_UDP_CONNRESET, &reportResets, sizeof(reportResets), nullptr, 0,
             &returned, nullptr, nullptr);

    m_port = rtspPort;
    m_log.Write("listening on port %u, RTP from UDP %u, RTCP %u", rtspPort, m_rtpPort,
                m_rtcpPort);
    return true;
}

void FrameStreamServer::Stop() {
    m_stopping = true;
    m_mailbox.Wake();
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    if (m_encodeThread.joinable()) {
        m_encodeThread.join();
    }
    ReapConnections(true);
    m_discovery.Stop();
    m_pointerReceiver.Stop();
    if (m_output) {
        m_output->Stop();
        m_output.reset();
    }
    m_listen.Reset();
    m_rtp.Reset();
    m_rtcp.Reset();
    m_winsock.Shutdown();
    m_running = false;
    m_port = 0;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_sessions.clear();
        m_viewers = 0;
        m_appViewers = 0;
    }
    std::lock_guard<std::mutex> lock(m_urlsMutex);
    m_urls.clear();
}

std::vector<std::wstring> FrameStreamServer::Urls() {
    uint16_t port = m_port;
    if (!m_running || !port) {
        return {};  // USB has no port
    }
    std::lock_guard<std::mutex> lock(m_urlsMutex);
    uint64_t now = GetTickCount64();
    // Memoized: listing the adapters takes milliseconds, and the UI asks every frame.
    if (m_urls.empty() || now - m_urlsAtMs > kUrlRefreshMs) {
        std::vector<std::wstring> addresses;
        if (m_lan) {
            addresses = net::LanAddresses();
        }
        if (addresses.empty()) {
            addresses.push_back(L"127.0.0.1");
        }
        m_urls.clear();
        for (const auto& address : addresses) {
            m_urls.push_back(m_output ? m_output->Url(address)
                                      : L"rtsp://" + address + L":" + std::to_wstring(port) + L"/");
        }
        m_urlsAtMs = now;
    }
    return m_urls;
}

std::wstring FrameStreamServer::Status() const {
    return m_running && m_output ? m_output->Status() : std::wstring();
}

bool FrameStreamServer::NeedsUsbDriver() const {
    return m_running && m_output && m_output->NeedsUsbDriver();
}

std::vector<UsbPhone> FrameStreamServer::PhonesNeedingUsbDriver() const {
    return m_running && m_output ? m_output->PhonesNeedingDriver() : std::vector<UsbPhone>();
}

std::string FrameStreamServer::Describe() {
    std::string name = "Gazeora";
    wchar_t computer[256];
    DWORD size = 256;
    if (GetComputerNameExW(ComputerNameDnsHostname, computer, &size)) {
        char utf8[1024];
        int length = WideCharToMultiByte(CP_UTF8, 0, computer, int(size), utf8, sizeof(utf8),
                                         nullptr, nullptr);
        if (length > 0) {
            name.assign(utf8, size_t(length));
        }
    }
    const StreamProtocol protocol = m_runningProtocol;
    return std::string("{\"service\":\"gazeora\",\"version\":1,\"name\":") + JsonString(name) +
           ",\"protocol\":\"" + ProtocolId(protocol) + "\",\"port\":" + std::to_string(m_port) +
           ",\"path\":\"" + (protocol == StreamProtocol::Http ? "/stream.ts" : "/") +
           "\",\"codec\":\"" + (Codec() == VideoCodec::H265 ? "h265" : "h264") + "\"}";
}

void FrameStreamServer::SetOverlay(const OverlaySettings& overlay) {
    std::lock_guard<std::mutex> lock(m_overlayMutex);
    m_overlay = overlay;
}

OverlaySettings FrameStreamServer::GetOverlay() {
    std::lock_guard<std::mutex> lock(m_overlayMutex);
    return m_overlay;
}

void FrameStreamServer::SetVrLayout(const VrLayout& vr) {
    std::lock_guard<std::mutex> lock(m_vrMutex);
    m_vr = vr;
    m_pointerGate.SetVrOn(vr.enabled);
}

VrLayout FrameStreamServer::GetVrLayout() {
    std::lock_guard<std::mutex> lock(m_vrMutex);
    return m_vr;
}

std::wstring FrameStreamServer::LastError() {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_lastError;
}

void FrameStreamServer::SetLastError(const std::wstring& error) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_lastError = error;
}

std::wstring FrameStreamServer::EncoderName() {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_encoderName;
}

void FrameStreamServer::PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>& bgra,
                                  bool stereo) {
    m_mailbox.PostLive(width, height, RtpClock64(), bgra, stereo);
}

void FrameStreamServer::SetIdleFrame(uint32_t width, uint32_t height, std::vector<uint8_t> bgra) {
    m_mailbox.PostIdle(width, height, std::move(bgra));
}

void FrameStreamServer::AcceptLoop() {
    while (!m_stopping) {
        // Poll so Stop doesn't have to close the socket out from under accept().
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listen.Get(), &readable);
        timeval timeout = {0, 250 * 1000};
        if (select(0, &readable, nullptr, nullptr, &timeout) != 1) {
            ReapConnections(false);
            continue;
        }
        sockaddr_in peer = {};
        int peerLength = sizeof(peer);
        net::Socket client(accept(m_listen.Get(), reinterpret_cast<sockaddr*>(&peer), &peerLength));
        if (!client) {
            continue;
        }
        BOOL noDelay = TRUE;
        setsockopt(client.Get(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&noDelay),
                   sizeof(noDelay));
        client.SetSendBuffer(kTcpSendBufferBytes);
        client.SetNonBlocking();

        auto connection = std::make_unique<RtspConnection>(std::move(client), net::AddressText(peer));
        m_log.Write("%s connected", connection->Peer().c_str());
        RtspConnection* raw = connection.get();
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections.push_back(std::move(connection));
        raw->thread = std::thread([this, raw] { Serve(*raw); });
    }
}

void FrameStreamServer::ReapConnections(bool all) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        RtspConnection& connection = **it;
        // With `all`, Stop has set m_stopping, which the connection notices within a poll.
        if (all || connection.done) {
            if (connection.thread.joinable()) {
                connection.thread.join();
            }
            it = m_connections.erase(it);  // closes the socket
        } else {
            ++it;
        }
    }
}

void FrameStreamServer::UpdateViewers() {
    m_viewers = int(std::count_if(m_sessions.begin(), m_sessions.end(),
                                  [](const auto& entry) { return entry.second.playing; }));
    m_appViewers = int(std::count_if(m_sessions.begin(), m_sessions.end(), [](const auto& entry) {
        return entry.second.playing && entry.second.app;
    }));
}

void FrameStreamServer::Serve(RtspConnection& connection) {
    const SOCKET socket = connection.Socket();
    sockaddr_in peer = {}, local = {};
    int length = sizeof(peer);
    getpeername(socket, reinterpret_cast<sockaddr*>(&peer), &length);
    length = sizeof(local);
    getsockname(socket, reinterpret_cast<sockaddr*>(&local), &length);

    RtspReader reader;
    std::string request;
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
        // Interleaved data from the player is its RTCP receiver reports. Nothing is lost on
        // TCP, so there's no loss to read from them: the reader drops them.
        RtspReader::Item item = reader.Next(request);
        if (item == RtspReader::Item::Interleaved) {
            continue;
        }
        if (item == RtspReader::Item::Request) {
            if (!Respond(connection, request, peer, local)) {
                reason = "it sent something other than RTSP, or the reply failed";
                break;
            }
            continue;
        }
        if (item == RtspReader::Item::TooLarge) {
            reason = "its request was too large";
            break;
        }
        // Poll, as a blocking recv can't be interrupted from Stop on Windows. While RTP
        // waits to go out on this connection, wait for room for it too, and look again soon:
        // the encoder only adds to it.
        bool pending = connection.HasPending();
        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_SET(socket, &readable);
        FD_ZERO(&writable);
        FD_SET(socket, &writable);
        timeval timeout = {0, (connection.interleaved ? 5 : 250) * 1000};
        int ready = select(0, &readable, pending ? &writable : nullptr, nullptr, &timeout);
        if (ready < 0) {
            reason = "the connection failed";
            break;
        }
        if (pending && FD_ISSET(socket, &writable) && !connection.Flush()) {
            reason = "sending to it failed";
            break;
        }
        if (!FD_ISSET(socket, &readable)) {
            if (GetTickCount64() - lastRequestMs > kReceiveTimeoutMs) {
                reason = "it was silent past the session timeout";
                break;
            }
            continue;
        }
        int got = recv(socket, chunk, sizeof(chunk), 0);
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
        reader.Append(chunk, size_t(got));
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
    m_log.Write("%s disconnected after %llu s: %s", connection.Peer().c_str(),
                (unsigned long long)((GetTickCount64() - connectedMs) / 1000), reason);
    // The socket is closed by ReapConnections, after this thread is joined.
    shutdown(socket, SD_SEND);
    connection.done = true;
}

// RTSP methods, dispatched through a table: a new method is one handler and one entry.
const std::map<std::string, FrameStreamServer::RequestHandler>& FrameStreamServer::Handlers() {
    static const std::map<std::string, RequestHandler> handlers = {
        {"OPTIONS", &FrameStreamServer::OnOptions},
        {"DESCRIBE", &FrameStreamServer::OnDescribe},
        {"SETUP", &FrameStreamServer::OnSetup},
        {"PLAY", &FrameStreamServer::OnPlayPauseTeardown},
        {"PAUSE", &FrameStreamServer::OnPlayPauseTeardown},
        {"TEARDOWN", &FrameStreamServer::OnPlayPauseTeardown},
        {"GET_PARAMETER", &FrameStreamServer::OnKeepalive},
        {"SET_PARAMETER", &FrameStreamServer::OnKeepalive},
    };
    return handlers;
}

bool FrameStreamServer::Respond(RtspConnection& connection, const std::string& text,
                                const sockaddr_in& peer, const sockaddr_in& local) {
    RtspRequest request;
    if (!RtspRequest::Parse(text, request)) {
        return false;  // not RTSP
    }
    RequestContext context{connection, request, peer, local};
    auto handler = Handlers().find(request.method);
    if (handler == Handlers().end()) {
        return connection.Send(RtspResponse("501 Not Implemented", request).Serialize());
    }
    return (this->*handler->second)(context);
}

bool FrameStreamServer::OnOptions(RequestContext& context) {
    return context.connection.Send(
        RtspResponse("200 OK", context.request)
            .Header("Public",
                    "OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, SET_PARAMETER")
            .Serialize());
}

bool FrameStreamServer::OnKeepalive(RequestContext& context) {
    return context.connection.Send(RtspResponse("200 OK", context.request).Serialize());
}

bool FrameStreamServer::OnDescribe(RequestContext& context) {
    char ip[INET_ADDRSTRLEN] = "0.0.0.0";
    inet_ntop(AF_INET, &context.local.sin_addr, ip, sizeof(ip));
    const std::string payloadType = std::to_string(kRtpVideoPayloadType);
    const VideoCodec codec = Codec();
    std::string fmtp;
    {
        std::unique_lock<std::mutex> lock(m_statusMutex);
        auto hasParameterSets = [&] {
            return m_fmtp[int(codec)].find("sprop-") != std::string::npos;
        };
        if (!hasParameterSets()) {
            m_parameterSetsWanted = true;
            m_fmtpChanged.wait_for(lock, kParameterSetsWait, hasParameterSets);
            m_parameterSetsWanted = false;
        }
        fmtp = m_fmtp[int(codec)];
    }
    std::string sdp = "v=0\r\no=- " + std::to_string(m_ssrc) + " 1 IN IP4 " + ip +
                      "\r\ns=Gazeora\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n"
                      "m=video 0 RTP/AVP " +
                      payloadType + "\r\na=rtpmap:" + payloadType + " " +
                      RtpPayloadFormat::For(codec).EncodingName() + "/90000\r\n" +
                      (fmtp.empty() ? "" : "a=fmtp:" + payloadType + " " + fmtp + "\r\n") +
                      "a=control:track1\r\n";
    const std::string& url = context.request.url;
    std::string base = !url.empty() && url.back() == '/' ? url : url + "/";
    return context.connection.Send(RtspResponse("200 OK", context.request)
                                       .Header("Content-Base", base)
                                       .Body("application/sdp", std::move(sdp))
                                       .Serialize());
}

bool FrameStreamServer::OnSetup(RequestContext& context) {
    RtspConnection& connection = context.connection;
    // UDP unicast, or interleaved in this connection. No multicast: players fall back to
    // unicast when it's refused.
    TransportSpec transport;
    if (!TransportSpec::Parse(context.request.Header("Transport"), transport)) {
        return connection.Send(
            RtspResponse("461 Unsupported Transport", context.request).Serialize());
    }
    // Only the transport the protocol names. Players such as VLC try the other on a 461.
    if (transport.interleaved != (m_runningProtocol == StreamProtocol::RtspTcp)) {
        m_log.Write("%s asked for RTP over %s; the stream is %s", connection.Peer().c_str(),
                    transport.interleaved ? "TCP" : "UDP", ProtocolName(m_runningProtocol));
        return connection.Send(
            RtspResponse("461 Unsupported Transport", context.request).Serialize());
    }
    std::string id = context.request.SessionId();
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || it->second.owner != &connection) {
        static std::mt19937_64 random{std::random_device()()};
        id = Hex(random(), 16);
        it = m_sessions.emplace(id, Session{&connection}).first;
    }
    Session& session = it->second;
    session.tcp = transport.interleaved;
    session.app = PlayerIdentity::IsGazeoraApp(context.request.Header("User-Agent"));
    std::string replyTransport;
    if (transport.interleaved) {
        session.channel = transport.channel;
        connection.interleaved = true;
        replyTransport = "RTP/AVP/TCP;unicast;interleaved=" + std::to_string(transport.channel) +
                         "-" + std::to_string(transport.channel + 1);
        m_log.Write("%s set up RTP over TCP, interleaved on channel %u", connection.Peer().c_str(),
                    unsigned(transport.channel));
    } else {
        session.rtp = context.peer;
        session.rtp.sin_port = htons(transport.rtpPort);
        session.rtcp = context.peer;
        session.rtcp.sin_port = htons(transport.rtcpPort);
        replyTransport = "RTP/AVP;unicast;client_port=" + std::to_string(transport.rtpPort) +
                         "-" + std::to_string(transport.rtcpPort) +
                         ";server_port=" + std::to_string(m_rtpPort) + "-" +
                         std::to_string(m_rtcpPort);
        m_log.Write("%s set up RTP over UDP to ports %u-%u", connection.Peer().c_str(),
                    unsigned(transport.rtpPort), unsigned(transport.rtcpPort));
    }
    return connection.Send(
        RtspResponse("200 OK", context.request)
            .Header("Transport", replyTransport + ";ssrc=" + Hex(m_ssrc, 8))
            .Header("Session", id + ";timeout=" + std::to_string(kSessionTimeoutSeconds))
            .Serialize());
}

bool FrameStreamServer::OnPlayPauseTeardown(RequestContext& context) {
    RtspConnection& connection = context.connection;
    const std::string& method = context.request.method;
    std::string id = context.request.SessionId();
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(id);
    if (it == m_sessions.end() || it->second.owner != &connection) {
        return connection.Send(RtspResponse("454 Session Not Found", context.request).Serialize());
    }
    m_log.Write("%s: %s", connection.Peer().c_str(), method.c_str());
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
    RtspResponse response("200 OK", context.request);
    response.Header("Session", id);
    if (method == "PLAY") {
        response.Header("Range", "npt=0.000-");
    }
    return connection.Send(response.Serialize());
}

void FrameStreamServer::SendPackets(const std::vector<std::string>& packets, bool keyFrame,
                                    PacketPacer& pacer) {
    uint64_t now = GetTickCount64();
    bool reportDue = now - m_lastReportMs >= kSenderReportIntervalMs;
    for (const std::string& packet : packets) {
        ++m_packetsSent;
        m_octetsSent += uint32_t(packet.size() - kRtpHeaderSize);  // payload only
    }
    std::string report = BuildSenderReport(m_ssrc, m_packetsSent, m_octetsSent);
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
            RtspConnection& connection = *session.owner;
            bool full = connection.Backlog() > kTcpBacklogBytes;
            if (full || (session.waitingForKeyFrame && !keyFrame)) {
                if (!full && session.waitingForKeyFrame) {
                    m_keyFrameWanted = true;
                }
                session.waitingForKeyFrame = true;
                ++session.framesSkipped;
            } else {
                session.waitingForKeyFrame = false;
                ++session.framesSent;
                connection.QueueInterleaved(session.channel, packets, reportNow ? &report : nullptr);
            }
            // Skipped frames count like the loss a UDP player reports, so the bitrate comes
            // down for a connection that can't keep up.
            if (now - session.countedSinceMs >= 1000) {
                uint32_t frames = session.framesSent + session.framesSkipped;
                if (session.framesSkipped > 0) {
                    m_bitrate.Report(uint64_t(uintptr_t(&connection)),
                                     float(session.framesSkipped) / float(frames), now);
                    m_log.Write("%s skipped %u of %u frames: the connection can't keep up",
                                connection.Peer().c_str(), session.framesSkipped, frames);
                }
                session.framesSent = session.framesSkipped = 0;
                session.countedSinceMs = now;
            }
        }
    }
    size_t perBurst = PacketPacer::PacketsPerBurst(packets.size());
    for (size_t first = 0; first < packets.size(); first += perBurst) {
        if (first > 0) {
            pacer.WaitForNextBurst();
        }
        size_t end = std::min(first + perBurst, packets.size());
        for (const sockaddr_in& target : targets) {
            for (size_t i = first; i < end; ++i) {
                // A lost or refused packet only costs that frame.
                sendto(m_rtp.Get(), packets[i].data(), int(packets[i].size()), 0,
                       reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            }
        }
    }
    for (const sockaddr_in& target : reportTargets) {
        sendto(m_rtcp.Get(), report.data(), int(report.size()), 0,
               reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    }
    if (reportDue) {
        m_lastReportMs = now;
    }
}

void FrameStreamServer::ReadReceiverReports(uint64_t now) {
    if (!m_rtcp) {
        return;  // not RTSP
    }
    uint8_t packet[1500];
    sockaddr_in from;
    int fromLength = sizeof(from);
    int got;
    while ((got = recvfrom(m_rtcp.Get(), reinterpret_cast<char*>(packet), sizeof(packet), 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLength)) > 0) {
        for (const ReceptionReport& report : ParseReceiverReports(packet, size_t(got))) {
            if (report.source != m_ssrc) {
                continue;
            }
            // VLC's first report has the cumulative loss at -1 (one packet more than expected)
            // and a fraction of 98%, which would send the bitrate to the bottom of the
            // ladder; negative means no loss.
            float fraction = report.cumulativeLost < 0 ? 0.0f : report.fractionLost;
            m_bitrate.Report(ReportSource(from), fraction, now);
            m_log.Write("%s reports %.1f%% lost lately, %d in all, jitter %u ms",
                        net::AddressText(from).c_str(), report.fractionLost * 100.0,
                        report.cumulativeLost, report.jitter / 90);
            HeardFrom(from, now);
        }
        if (AsksForKeyFrame(packet, size_t(got), m_ssrc)) {
            m_keyFrameWanted = true;  // it lost part of a frame: the next one starts over
            HeardFrom(from, now);
        }
        fromLength = sizeof(from);
    }

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& entry : m_sessions) {
        Session& session = entry.second;
        if (session.playing && !session.tcp && !session.quietLogged &&
            now - session.lastHeardMs > kQuietPlayerMs) {
            session.quietLogged = true;
            m_log.Write("%s has sent no receiver report for %llu s: it may have stopped receiving",
                        session.owner->Peer().c_str(),
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
                m_log.Write("%s is reporting again", session.owner->Peer().c_str());
            }
            session.lastHeardMs = now;
            session.quietLogged = false;
        }
    }
}

uint32_t FrameStreamServer::BitrateAt(int step) const {
    uint32_t top = m_maxBitrate / 100 * (Codec() == VideoCodec::H265 ? kH265BitratePercent : 100);
    return BitrateController::BitrateAt(step, top);
}

uint32_t FrameStreamServer::Bitrate() const { return BitrateAt(m_bitrate.Step()); }

uint32_t FrameStreamServer::FullBitrate() const { return BitrateAt(0); }

void FrameStreamServer::SetCodec(VideoCodec codec) {
    VideoCodec before = Codec();
    m_requestedCodec = codec;
    if (Codec() != before) {
        ++m_codecGeneration;
        m_log.Write("codec now %s; players were disconnected", CodecName(Codec()));
    }
}

VideoCodec FrameStreamServer::Codec() const {
    return m_requestedCodec == VideoCodec::H265 && m_h265Available ? VideoCodec::H265
                                                                   : VideoCodec::H264;
}

bool FrameStreamServer::CodecAvailable(VideoCodec codec) const {
    return codec == VideoCodec::H264 || m_h265Available;
}

void FrameStreamServer::AdaptQuality(uint64_t now) {
    if (m_bitrate.Update(now, m_viewers) && m_viewers > 0) {
        m_log.Write("bitrate now %.1f Mbit/s; worst loss reported lately %.1f%%",
                    BitrateAt(m_bitrate.Step()) / 1e6, m_bitrate.WorstLoss() * 100.0);
    }
}

bool FrameStreamServer::StreamPicture(VideoPipeline& pipeline, const Picture& picture,
                                      PacketPacer& pacer, bool send) {
    VideoPipeline::Config config;
    config.codec = Codec();
    config.fps = kFrameRate;
    config.bitrate = BitrateAt(m_bitrate.Step());
    config.keyInterval = kKeyFrameInterval;
    config.ssrc = m_ssrc;
    config.maxPacketSize = kMaxPacketSize;
    // Encoders take even sizes only.
    const uint32_t width = picture.width & ~1u, height = picture.height & ~1u;

    std::wstring error;
    bool reopened = false;
    if (!pipeline.Prepare(config, width, height, reopened, error)) {
        m_log.Write("cannot open an %s encoder for %ux%u: %ls", CodecName(config.codec), width,
                    height, error.c_str());
        if (config.codec == VideoCodec::H265) {
            // Listed but not working (a size it doesn't take, a driver problem): fall back to
            // H.264, which always has the software encoder.
            VideoCodec before = Codec();
            m_h265Available = false;
            if (Codec() != before) ++m_codecGeneration;
        }
        SetLastError(error);
        return false;
    }
    if (reopened) {
        std::wstring name = pipeline.EncoderName();
        m_log.Write("encoding %s %ux%u with %ls at %.1f Mbit/s", CodecName(config.codec), width,
                    height, name.c_str(), config.bitrate / 1e6);
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_encoderName = std::move(name);
    }
    if (m_keyFrameWanted.exchange(false)) {
        pipeline.RequestKeyFrame();
    }
    pipeline.SetViewInfo(ViewInfoText(GetVrLayout(), picture.stereo));

    bool fmtpChanged = false;
    bool ok = pipeline.Encode(
        picture,
        [&](const VideoPipeline::EncodedFrame& frame) {
            if (!send) {
                return;
            }
            if (m_output) {
                m_output->Send(frame, m_bitrate, GetTickCount64());
            } else {
                SendPackets(frame.packets, frame.keyFrame, pacer);
            }
        },
        fmtpChanged, error);
    if (fmtpChanged) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_fmtp[int(pipeline.Codec())] = pipeline.Fmtp();
        m_fmtpChanged.notify_all();
    }
    SetLastError(ok ? std::wstring() : error);
    if (!ok) {
        m_log.Write("encoding failed: %ls", error.c_str());  // reopened with the next frame
    }
    return ok;
}

void FrameStreamServer::EncodeLoop() {
    EncodeThreadRuntime runtime;
    if (!runtime.MediaFoundation()) {
        SetLastError(L"Media Foundation is unavailable (Windows N without the Media Feature Pack?)");
        return;
    }
    PacketPacer pacer;
    VideoPipeline pipeline(m_encoderFactory);
    Picture live, idle;
    Picture overlaid;  // the idle picture with the overlay drawn on
    OverlayRenderer overlay;
    FrameRateCounter frameRate;
    uint64_t lastLiveMs = 0;
    bool probed = false;  // the idle frame was encoded with no one watching
    uint64_t nextIdleMs = 0;  // on the 90 kHz clock's millisecond scale; GetTickCount64 is too
                              // coarse for 33 ms

    while (true) {
        // Until a frame arrives or the next idle frame is due.
        uint64_t clockMs = RtpClock64() / 90;
        uint64_t waitMs = nextIdleMs > clockMs ? nextIdleMs - clockMs : 0;
        bool gotLive = m_mailbox.Take(DWORD(std::min(waitMs, kIdleIntervalMs)), live, idle);
        if (m_stopping) {
            break;
        }

        uint64_t now = GetTickCount64();
        if (m_output) {
            m_viewers = m_output->Viewers();
            m_appViewers = m_output->AppViewers();
            if (m_output->TakeKeyFrameRequest()) {
                m_keyFrameWanted = true;
            }
        }
        ReadReceiverReports(now);
        AdaptQuality(now);

        // The idle frame goes through the encoder too, like a game that shows a still picture.
        const Picture* source = nullptr;
        clockMs = RtpClock64() / 90;  // after the wait
        if (gotLive) {
            source = &live;
            lastLiveMs = now;
            nextIdleMs = clockMs + kIdleIntervalMs;
        } else if (!idle.bgra.empty() && (m_viewers > 0 || m_parameterSetsWanted) &&
                   now - lastLiveMs >= kLiveTimeoutMs) {
            if (clockMs >= nextIdleMs) {
                source = &idle;
                idle.clock = RtpClock64();
                // One interval on from when this one was due, so the time spent encoding
                // doesn't add up; more than an interval behind (after a pause), from now.
                nextIdleMs = clockMs - nextIdleMs > kIdleIntervalMs ? clockMs + kIdleIntervalMs
                                                                    : nextIdleMs + kIdleIntervalMs;
            }
        } else {
            nextIdleMs = clockMs + kIdleIntervalMs;  // nothing to repeat: don't spin
        }
        if (source && source->Complete()) {
            // The idle frame with no one watching is only for a DESCRIBE's parameter sets: it
            // isn't sent, and the first player gets a fresh encoder, so its first frame is a
            // key frame rather than one still in the encoder.
            bool probe = source == &idle && m_viewers == 0;
            if (probed && !probe) {
                pipeline.Close();
                probed = false;
            }
            if (!probe) {
                frameRate.Add(now);
                source = DrawOverlay(*source, source == &idle ? overlaid : live, overlay,
                                     frameRate.PerSecond(now));
            }
            StreamPicture(pipeline, *source, pacer, !probe);
            probed |= probe;
        }
    }
    pipeline.Close();
}

const Picture* FrameStreamServer::DrawOverlay(const Picture& picture, Picture& target,
                                              OverlayRenderer& renderer, float frameRate) {
    OverlaySettings settings = GetOverlay();
    if (!settings.enabled) {
        return &picture;
    }
    OverlayStats stats;
    stats.protocol = ProtocolName(m_runningProtocol);
    stats.viewers = m_viewers;
    stats.codec = CodecName(Codec());
    stats.width = picture.width & ~1u;  // as encoded
    stats.height = picture.height & ~1u;
    stats.frameRate = frameRate;
    stats.bitrate = Bitrate();
    stats.loss = PacketLoss();
    stats.encoder = EncoderName();
    GetLocalTime(&stats.time);
    std::vector<std::wstring> lines = OverlayLines(settings, stats);
    if (lines.empty()) {
        return &picture;
    }
    // The idle picture is reused frame after frame: draw on a copy of it.
    if (&target != &picture) {
        target.bgra.assign(picture.bgra.begin(), picture.bgra.end());
        target.width = picture.width;
        target.height = picture.height;
        target.clock = picture.clock;
        target.stereo = picture.stereo;
    }
    OverlayLayout layout = GetVrLayout().WarpOnPc() ? OverlayLayout::VrEyes
                           : picture.stereo         ? OverlayLayout::Pair
                                                    : OverlayLayout::Flat;
    renderer.Draw(lines, settings, layout, target);
    return &target;
}

}  // namespace gazeora
