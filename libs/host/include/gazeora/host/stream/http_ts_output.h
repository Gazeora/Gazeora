#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/rtsp_connection.h"
#include "gazeora/host/stream/ts_muxer.h"

namespace gazeora {

// The HTTP protocol: GET /stream.ts (or /) answers with a live MPEG transport stream that
// never ends (ts_muxer.h). A player starts on the next key frame. One that falls behind skips
// frames up to the next key frame, like an RTSP player over TCP, and the skips count as loss
// for the bitrate.
class HttpTsOutput : public FrameOutput {
public:
    explicit HttpTsOutput(StreamLog& log) : m_log(log) {}
    ~HttpTsOutput() override { Stop(); }

    bool Start(in_addr address, uint16_t port, std::wstring& error) override;
    void Stop() override;
    uint16_t Port() const override { return m_port; }
    int Viewers() const override { return m_viewers; }
    int AppViewers() const override { return m_appViewers; }
    bool TakeKeyFrameRequest() override { return m_keyFrameWanted.exchange(false); }
    void Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
              uint64_t now) override;
    std::wstring Url(const std::wstring& host) const override;

private:
    // One HTTP connection. Its queue (RtspConnection is a plain non-blocking connection with
    // an output queue) takes the stream once the request asked for it.
    struct Player {
        std::unique_ptr<RtspConnection> connection;
        bool streaming = false;
        bool app = false;  // the Gazeora app, by its User-Agent
        bool waitingForKeyFrame = true;
        uint32_t framesSent = 0;
        uint32_t framesSkipped = 0;
        uint64_t countedSinceMs = 0;
    };

    void AcceptLoop();
    void Serve(RtspConnection& connection);
    // Reads and answers the request; true if the player now gets the stream.
    bool Answer(RtspConnection& connection, const std::string& request);
    void SetStreaming(RtspConnection& connection, bool streaming, bool app = false);
    void Reap(bool all);

    StreamLog& m_log;
    net::Socket m_listen;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<int> m_viewers{0};
    std::atomic<int> m_appViewers{0};
    std::atomic<bool> m_keyFrameWanted{false};
    std::thread m_acceptThread;

    std::mutex m_playersMutex;
    std::list<Player> m_players;

    // The encode thread's.
    TsMuxer m_muxer;
    std::string m_ts;
};

}  // namespace gazeora
