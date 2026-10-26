#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gazeora/host/capture/vr_layout.h"
#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/stream/bitrate_controller.h"
#include "gazeora/host/stream/discovery.h"
#include "gazeora/host/stream/frame_mailbox.h"
#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_source.h"
#include "gazeora/host/stream/live_frame_sink.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/pointer_gate.h"
#include "gazeora/host/stream/pointer_input_receiver.h"
#include "gazeora/host/stream/stream_log.h"
#include "gazeora/host/stream/stream_overlay.h"
#include "gazeora/host/stream/stream_protocol.h"
#include "gazeora/host/stream/video_pipeline.h"

namespace gazeora {

class RtspConnection;
class PacketSendQueue;
struct RtspRequest;

// Streams captured frames to players, over one protocol at a time (stream_protocol.h):
//  - RTSP: a server (TCP) that players such as VLC, ffplay or OBS open at
//    rtsp://<this PC>:<port>/, and H.264 or H.265 over RTP (RFC 6184, RFC 7798) to every
//    player that pressed play. Over UDP, a lost packet smears the picture until the next key
//    frame, at most a second later. Interleaved in the RTSP connection (RFC 2326 10.12),
//    nothing is lost, and a player that falls behind skips frames up to the next key frame.
//    The protocol picks which of the two players may set up; the other is refused.
//  - UDP, HTTP and USB: a FrameOutput each (frame_output.h).
// Whatever the protocol, the PC answers the Android app's search for it (discovery.h).
// Frames are pushed from the capture thread and encoded on the server's own thread; frames
// arriving faster than they can be encoded are dropped, newest wins. They come as BGRA8 in
// memory, or as NV12 textures that go to a GPU encoder without leaving the GPU. While no frame has
// been pushed for a second, the idle frame (Gazeora's window) is streamed instead.
//
// The server coordinates; the work is done by its parts: FrameMailbox (frames in),
// VideoPipeline (encoding, RTP), BitrateController (adapting to loss), RtspConnection (one
// per RTSP player), FrameOutput (the other protocols), PacketSendQueue (spreading UDP packets),
// DiscoveryResponder and StreamLog.
class FrameStreamServer : public LiveFrameSink {
public:
    // Frames should be scaled down to fit before they're pushed (TextureReader::SetMaxSize):
    // 1080p at several Mbit/s is what phones on Wi-Fi handle well.
    static constexpr uint32_t kMaxWidth = 1920;
    static constexpr uint32_t kMaxHeight = 1080;

    // `encoderFactory` supplies the video encoder; the default is the Media Foundation one.
    explicit FrameStreamServer(VideoPipeline::EncoderFactory encoderFactory = CreateVideoEncoder);
    FrameStreamServer(const FrameStreamServer&) = delete;
    FrameStreamServer& operator=(const FrameStreamServer&) = delete;
    ~FrameStreamServer() override;

    // Starts the protocol (SetProtocol), listening on `port` (0 picks a free one) if it has
    // one, on every network interface when `lan` is set and on 127.0.0.1 only otherwise.
    bool Start(uint16_t port, bool lan, std::wstring& error);
    void Stop();

    // The protocol the next Start runs; RTSP over TCP unless set. Stop and Start again to
    // switch while running: players watching are disconnected.
    void SetProtocol(StreamProtocol protocol) { m_protocol = protocol; }
    StreamProtocol Protocol() const { return m_protocol; }
    // Where the discovery responder listens: kDiscoveryPort unless set before Start (0 picks
    // a free port, for tests). Its port while running, 0 if it couldn't start.
    void SetDiscoveryPort(uint16_t port) { m_discoveryPort = port; }
    uint16_t DiscoveryPort() const { return m_discovery.Port(); }

    bool Running() const { return m_running; }
    uint16_t Port() const { return m_port; }  // 0 for USB, which has none
    // What players open to watch: one URL per network address of this PC, 127.0.0.1 when it
    // has none (or the server is loopback only). Empty when not running, and for USB.
    std::vector<std::wstring> Urls();
    // How the USB connection stands; empty for the other protocols.
    std::wstring Status() const;
    // Whether a phone on USB waits for the driver Gazeora installs.
    bool NeedsUsbDriver() const;
    // The phones on USB it can't switch to accessory mode without that driver on them.
    std::vector<UsbPhone> PhonesNeedingUsbDriver() const;
    // Players currently receiving the stream.
    int Viewers() const { return m_viewers; }
    // Of those, the ones that are the Gazeora Android app, not VLC and the like
    // (player_identity.h).
    int AppViewers() const { return m_appViewers; }
    // False while nobody watches; frames pushed then are wasted work.
    bool WantsFrames() const override { return m_viewers > 0; }
    // The overlay is drawn into pixels in memory, so it needs frames pushed as BGRA8.
    bool WantsTextures() override { return !GetOverlay().enabled; }
    // Why the last pushed frame couldn't be streamed; empty if it was.
    std::wstring LastError();
    // The share of packets players reported lost lately (worst player, 0..1), and the
    // bitrate the stream has backed off to because of it, in bits/s.
    float PacketLoss() const { return m_bitrate.WorstLoss(); }
    uint32_t Bitrate() const;
    uint32_t FullBitrate() const;  // for the codec in use
    // The encoder in use, e.g. "NVIDIA H.264 Encoder MFT"; empty before the first frame.
    std::wstring EncoderName();

    // The codec to stream in. H.265 only takes effect when this PC has an encoder for it
    // (CodecAvailable); otherwise the stream stays H.264. A change disconnects the players
    // watching, as they can't switch codecs mid-stream; they have to open the stream again.
    void SetCodec(VideoCodec codec);
    VideoCodec Codec() const;                     // the one players get
    bool CodecAvailable(VideoCodec codec) const;  // known once the server has started

    // The bitrate at full quality, for H.264, in bits/s (H.265 gets 70% of it, for about the
    // same picture). The stream steps down from it while players report loss. Applies from the
    // next frame.
    static constexpr uint32_t kDefaultMaxBitrate = 20000000;
    void SetMaxBitrate(uint32_t bitrate) { m_maxBitrate = bitrate; }
    uint32_t MaxBitrate() const { return m_maxBitrate; }

    // Frames streamed a second, a key frame among them every second. 60 halves the wait for
    // the next frame and moves smoother than 30, from the same bitrate: each frame gets half
    // the bits. A change opens a new encoder, so players get a key frame; it applies from the
    // next frame. Capture follows it (GameSession).
    static constexpr uint32_t kDefaultFrameRate = 60;
    static constexpr uint32_t kMinFrameRate = 10;
    static constexpr uint32_t kMaxFrameRate = 120;
    void SetFrameRate(uint32_t fps);
    uint32_t FrameRate() const { return m_frameRate; }

    // The layout frames should be pushed in. The server only stores it: whoever reads the
    // frames back applies it (TextureReader::SetVrLayout), once per frame.
    void SetVrLayout(const VrLayout& vr);
    VrLayout GetVrLayout();

    // Where the Gazeora app's touches go (pointer_message.h), from the server's threads: over
    // the network on kPointerPort, or on the USB cable. Only while the VR layout is off; null
    // drops them. `sink` must outlive the server.
    void SetPointerSink(PointerEventSink* sink) { m_pointerGate.SetTarget(sink); }

    // The streaming details drawn into every frame streamed, live or idle (stream_overlay.h).
    void SetOverlay(const OverlaySettings& overlay);
    OverlaySettings GetOverlay();

    // Hands over one tightly packed BGRA8 frame; `stereo` when it holds two views side by
    // side (TextureReader::Stereo). `bgra` is swapped with an older buffer, so the caller can
    // reuse it for the next frame without reallocating.
    void PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>& bgra,
                   bool stereo = false) override;
    // One NV12 frame on the GPU, width x height (even), from GpuNv12Converter. Encoded on the
    // GPU when an encoder there takes its device's textures, read back otherwise.
    void PushTexture(uint32_t width, uint32_t height, Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12,
                     bool stereo = false) override;
    // The last frame pushed, encoded again: for a game drawing fewer frames than the stream
    // sends, with nothing read back or converted.
    void RepeatFrame() override;
    // The BGRA8 picture streamed while there are no live frames.
    void SetIdleFrame(uint32_t width, uint32_t height, std::vector<uint8_t> bgra);

    // Writes what happens to players (connections, transports, reported loss, bitrate
    // changes, skipped frames) to `path`, replacing what was there. Empty stops logging.
    void SetLogFile(const std::wstring& path) { m_log.Open(path); }

private:
    // A player that has set up the stream, by RTSP session id.
    struct Session {
        RtspConnection* owner;
        in_addr peer = {};    // the player's address
        sockaddr_in rtp;      // where the player wants the RTP packets (UDP)
        sockaddr_in rtcp;     // and the RTCP sender reports
        bool tcp = false;     // interleaved in the RTSP connection instead
        uint8_t channel = 0;  // interleaved channel for RTP; RTCP goes on the next one
        bool playing = false;
        bool app = false;          // the Gazeora app, by its User-Agent
        bool needsReport = false;  // just started playing: send a sender report right away
        // Interleaved: frames that didn't fit the connection's backlog, and whether the
        // player has to wait for a key frame since.
        bool waitingForKeyFrame = false;
        uint32_t framesSent = 0;
        uint32_t framesSkipped = 0;
        uint64_t countedSinceMs = 0;
        // UDP: when the player last sent a receiver report, so a player that went quiet
        // shows up in the log.
        uint64_t lastHeardMs = 0;
        bool quietLogged = false;
    };

    // What an RTSP request handler gets.
    struct RequestContext {
        RtspConnection& connection;
        const RtspRequest& request;
        const sockaddr_in& peer;
        const sockaddr_in& local;
    };
    using RequestHandler = bool (FrameStreamServer::*)(RequestContext&);

    void AcceptLoop();
    void EncodeLoop();
    void Serve(RtspConnection& connection);
    // Answers one RTSP request; false to close the connection.
    bool Respond(RtspConnection& connection, const std::string& text, const sockaddr_in& peer,
                 const sockaddr_in& local);
    bool OnOptions(RequestContext& context);
    bool OnDescribe(RequestContext& context);
    bool OnSetup(RequestContext& context);
    bool OnPlayPauseTeardown(RequestContext& context);
    bool OnKeepalive(RequestContext& context);
    static const std::map<std::string, RequestHandler>& Handlers();

    void ReapConnections(bool all);
    void UpdateViewers();  // with m_sessionsMutex held
    // Sends a frame's packets to every player, and the RTCP sender report when one is due.
    void SendPackets(const VideoPipeline::EncodedFrame& frame);
    // Collects the loss players report in RTCP for the bitrate controller.
    void ReadReceiverReports(uint64_t now);
    void HeardFrom(const sockaddr_in& from, uint64_t now);  // a UDP player's report
    void AdaptQuality(uint64_t now);
    uint32_t BitrateAt(int step) const;  // on the ladder, for the codec in use
    // Encodes one picture and streams it, or only encodes it when !send (for the parameter
    // sets). False if it couldn't be.
    bool StreamPicture(VideoPipeline& pipeline, const Picture& picture, FrameSource source,
                       bool send);
    // `picture` with the overlay drawn on, when it's on: drawn in `target`, a copy, so the
    // picture itself can be encoded again. `picture` as it is otherwise, and for textures.
    const Picture* DrawOverlay(const Picture& picture, Picture& target, OverlayRenderer& renderer,
                               float frameRate);
    void SetLastError(const std::wstring& error);
    bool StartRtsp(in_addr address, uint16_t port, std::wstring& error);
    // Whether a player at `address` is watching, on whichever protocol (FrameOutput::Watching).
    bool Watching(const in_addr& address);
    // The discovery reply (discovery.h).
    std::string Describe();

    VideoPipeline::EncoderFactory m_encoderFactory;
    net::WinsockRuntime m_winsock;
    bool m_lan = false;
    std::atomic<StreamProtocol> m_protocol{StreamProtocol::RtspTcp};
    std::atomic<StreamProtocol> m_runningProtocol{StreamProtocol::RtspTcp};  // as started
    std::atomic<bool> m_running{false};
    // The protocol's output when it isn't RTSP. Set while stopped only.
    std::unique_ptr<FrameOutput> m_output;
    DiscoveryResponder m_discovery{m_log};
    uint16_t m_discoveryPort = kDiscoveryPort;
    net::Socket m_listen;
    net::Socket m_rtp;
    net::Socket m_rtcp;  // bound so players' RTCP reports have somewhere to go
    // Sends RTP and RTCP over UDP paced, off the encode thread. While RTSP runs only.
    std::unique_ptr<PacketSendQueue> m_sendQueue;
    uint16_t m_rtpPort = 0;
    uint16_t m_rtcpPort = 0;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<int> m_viewers{0};
    std::atomic<int> m_appViewers{0};
    uint32_t m_ssrc = 0;
    // Sender statistics for RTCP, touched by the encode thread only.
    uint32_t m_packetsSent = 0;
    uint32_t m_octetsSent = 0;
    uint64_t m_lastReportMs = 0;
    // Reports and Update from the encode thread; its readers from any.
    BitrateController m_bitrate;
    std::atomic<bool> m_keyFrameWanted{false};  // a player just started: send one now
    std::atomic<VideoCodec> m_requestedCodec{VideoCodec::H264};
    std::atomic<uint32_t> m_maxBitrate{kDefaultMaxBitrate};
    std::atomic<uint32_t> m_frameRate{kDefaultFrameRate};
    std::atomic<bool> m_h265Available{false};
    // Bumped when the codec players get changes; their connections close on seeing it.
    std::atomic<uint32_t> m_codecGeneration{0};
    std::thread m_acceptThread;
    std::thread m_encodeThread;

    std::mutex m_connectionsMutex;
    std::list<std::unique_ptr<RtspConnection>> m_connections;

    std::mutex m_sessionsMutex;
    std::map<std::string, Session> m_sessions;  // by RTSP session id

    // Frames waiting for the encoder. Lives as long as the server, so an idle frame set
    // before Start isn't lost.
    FrameMailbox m_mailbox;

    // What the encode thread reports back.
    std::mutex m_statusMutex;
    std::wstring m_lastError;
    std::wstring m_encoderName;
    // SDP fmtp parameters by codec, from the encoder's parameter sets.
    std::string m_fmtp[2] = {"packetization-mode=1", ""};
    std::condition_variable m_fmtpChanged;  // with m_statusMutex
    // A DESCRIBE came before anything was encoded: encode the idle frame for its parameter
    // sets even with no viewers.
    std::atomic<bool> m_parameterSetsWanted{false};

    std::mutex m_vrMutex;
    VrLayout m_vr;

    PointerGate m_pointerGate;
    PointerInputReceiver m_pointerReceiver{
        m_pointerGate, m_log, [this](const in_addr& address) { return Watching(address); }};

    std::mutex m_overlayMutex;
    OverlaySettings m_overlay;

    std::mutex m_urlsMutex;
    std::vector<std::wstring> m_urls;
    uint64_t m_urlsAtMs = 0;

    StreamLog m_log;
};

}  // namespace gazeora
