#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/capture/vr_layout.h"

namespace gazeora {

// Streams captured frames to players on the local network: an RTSP server (TCP) that
// players such as VLC, ffplay or OBS open at rtsp://<this PC>:<port>/, and H.264 or H.265
// over RTP (RFC 6184, RFC 7798) to every player that pressed play. Over UDP by default, where a lost packet
// smears the picture until the next key frame, at most a second later. Players that ask
// for it get RTP interleaved in the RTSP connection instead (RFC 2326 10.12): nothing is
// lost, and a player that falls behind skips frames up to the next key frame. Frames are
// pushed from the capture thread and encoded on the server's own thread; frames arriving
// faster than they can be encoded are dropped, newest wins. While no frame has been pushed
// for a second, the idle frame (the logo) is streamed instead.
class FrameStreamServer {
public:
    // Frames should be scaled down to fit before they're pushed (TextureReader::SetMaxSize):
    // 1080p at several Mbit/s is what phones on Wi-Fi handle well.
    static constexpr uint32_t kMaxWidth = 1920;
    static constexpr uint32_t kMaxHeight = 1080;

    FrameStreamServer() = default;
    FrameStreamServer(const FrameStreamServer&) = delete;
    FrameStreamServer& operator=(const FrameStreamServer&) = delete;
    ~FrameStreamServer() {
        Stop();
        SetLogFile({});
    }

    // Starts listening on `port` (0 picks a free one), on every network interface when
    // `lan` is set and on 127.0.0.1 only otherwise.
    bool Start(uint16_t port, bool lan, std::wstring& error);
    void Stop();

    bool Running() const { return m_port != 0; }
    uint16_t Port() const { return m_port; }
    // rtsp:// URLs players can open: one per network address of this PC, 127.0.0.1 when
    // it has none (or the server is loopback only). Empty when not running.
    std::vector<std::wstring> Urls();
    // Players currently receiving the stream.
    int Viewers() const { return m_viewers; }
    // False while nobody watches; frames pushed then are wasted work.
    bool WantsFrames() const { return m_viewers > 0; }
    // Why the last pushed frame couldn't be streamed; empty if it was.
    std::wstring LastError();
    // The share of packets players reported lost lately (worst player, 0..1), and the
    // bitrate the stream has backed off to because of it, in bits/s.
    float PacketLoss() const { return m_packetLoss; }
    uint32_t Bitrate() const;
    uint32_t FullBitrate() const;  // for the codec in use
    // The encoder in use, e.g. "NVIDIA H.264 Encoder MFT"; empty before the first frame.
    std::wstring EncoderName();

    // The codec to stream in. H.265 only takes effect when this PC has an encoder for it
    // (CodecAvailable); otherwise the stream stays H.264. A change disconnects the players
    // watching, as they can't switch codecs mid-stream; they have to open the stream again.
    void SetCodec(VideoCodec codec);
    VideoCodec Codec() const;  // the one players get
    bool CodecAvailable(VideoCodec codec) const;  // known once the server has started

    // The layout frames should be pushed in. The server only stores it: whoever reads the
    // frames back applies it (TextureReader::SetVrLayout), once per frame.
    void SetVrLayout(const VrLayout& vr);
    VrLayout GetVrLayout();

    // Hands over one tightly packed BGRA8 frame. `bgra` is swapped with an older buffer,
    // so the caller can reuse it for the next frame without reallocating.
    void PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>& bgra);
    // The BGRA8 picture streamed while there are no live frames.
    void SetIdleFrame(uint32_t width, uint32_t height, std::vector<uint8_t> bgra);

    // Writes what happens to players (connections, transports, reported loss, bitrate
    // changes, skipped frames) to `path`, replacing what was there. Empty stops logging.
    void SetLogFile(const std::wstring& path);

private:
    struct Connection {
        SOCKET socket = INVALID_SOCKET;  // non-blocking
        std::string peer;                // "address:port", for the log
        std::thread thread;
        std::atomic<bool> done{false};
        std::atomic<bool> interleaved{false};  // RTP goes out on it
        // Everything sent on the socket queues here: RTSP replies from the connection's
        // thread and interleaved RTP from the encoder. A slow player can't hold either up.
        std::mutex outMutex;
        std::string out;
    };
    struct Session {
        Connection* owner;
        sockaddr_in rtp;   // where the player wants the RTP packets (UDP)
        sockaddr_in rtcp;  // and the RTCP sender reports
        bool tcp = false;  // interleaved in the RTSP connection instead
        uint8_t channel = 0;  // interleaved channel for RTP; RTCP goes on the next one
        bool playing = false;
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

    void AcceptLoop();
    void EncodeLoop();
    void Serve(Connection& connection);
    // Answers one RTSP request; false to close the connection.
    bool Respond(Connection& connection, const std::string& request, const sockaddr_in& peer,
                 const sockaddr_in& local);
    void ReapConnections(bool all);
    void UpdateViewers();  // with m_sessionsMutex held
    // Sends a frame's packets to every player, and the RTCP sender report when one is due.
    void SendPackets(const std::vector<std::string>& packets, bool keyFrame);
    // Queues `data` on the connection and sends what the socket takes without blocking.
    // False if the connection is broken.
    static bool Send(Connection& connection, const std::string& data);
    static bool Flush(Connection& connection);  // with outMutex held
    void Log(const char* format, ...);
    // Collects the loss players report in RTCP and adjusts m_quality to it.
    void ReadReceiverReports(uint64_t now);
    void HeardFrom(const sockaddr_in& from, uint64_t now);  // a UDP player's report
    void AdaptQuality(uint64_t now);
    uint32_t BitrateAt(int level) const;  // kBitrates[level], for the codec in use

    bool m_lan = false;
    SOCKET m_listen = INVALID_SOCKET;
    SOCKET m_rtp = INVALID_SOCKET;
    SOCKET m_rtcp = INVALID_SOCKET;  // bound so players' RTCP reports have somewhere to go
    uint16_t m_rtpPort = 0;
    uint16_t m_rtcpPort = 0;
    bool m_wsaStarted = false;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<int> m_viewers{0};
    uint32_t m_ssrc = 0;
    // Sender statistics for RTCP, touched by the encode thread only.
    uint32_t m_packetsSent = 0;
    uint32_t m_octetsSent = 0;
    uint64_t m_lastReportMs = 0;
    HANDLE m_paceTimer = nullptr;
    // Loss reports by player RTCP address, and the quality they drive. Encode thread only,
    // apart from the two atomics.
    struct LossReport {
        float fraction;
        uint64_t atMs;
    };
    std::map<uint64_t, LossReport> m_lossReports;
    uint64_t m_lastLossMs = 0;
    uint64_t m_lastQualityChangeMs = 0;
    std::atomic<float> m_packetLoss{0.0f};
    std::atomic<int> m_qualityLevel{0};         // index into kBitrates
    std::atomic<bool> m_keyFrameWanted{false};  // a player just started: send one now
    std::atomic<VideoCodec> m_requestedCodec{VideoCodec::H264};
    std::atomic<bool> m_h265Available{false};
    // Bumped when the codec players get changes; their connections close on seeing it.
    std::atomic<uint32_t> m_codecGeneration{0};
    std::thread m_acceptThread;
    std::thread m_encodeThread;

    std::mutex m_connectionsMutex;
    std::list<std::unique_ptr<Connection>> m_connections;

    std::mutex m_sessionsMutex;
    std::map<std::string, Session> m_sessions;  // by RTSP session id

    // Frames waiting for the encoder.
    std::mutex m_mutex;
    // Signaled when a frame arrives or the server stops. A Win32 event rather than a condition
    // variable: MinGW's wait_for(33 ms) takes about 47, which halves the idle frame rate.
    HANDLE m_frameReady = nullptr;
    std::vector<uint8_t> m_raw;
    uint32_t m_rawWidth = 0;
    uint32_t m_rawHeight = 0;
    uint64_t m_rawClock = 0;  // 90 kHz clock when pushed: the frame's capture time
    bool m_rawPending = false;
    std::vector<uint8_t> m_idle;
    uint32_t m_idleWidth = 0;
    uint32_t m_idleHeight = 0;
    bool m_idlePending = false;
    std::wstring m_lastError;
    std::wstring m_encoderName;
    // SDP fmtp parameters by codec, from the encoder's parameter sets.
    std::string m_fmtp[2] = {"packetization-mode=1", ""};

    std::mutex m_vrMutex;
    VrLayout m_vr;

    std::mutex m_urlsMutex;
    std::vector<std::wstring> m_urls;
    uint64_t m_urlsAtMs = 0;

    std::mutex m_logMutex;
    FILE* m_log = nullptr;
    long m_logBytes = 0;
};

}  // namespace gazeora
