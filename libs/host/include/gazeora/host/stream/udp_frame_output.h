#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/packet_pacer.h"

namespace gazeora {

// The UDP protocol: each frame cut into GZF1 datagrams (frame_packet.h), sent to every
// player that said hello lately. Players say where they are by saying hello, so the frames go
// back the way the hello came: through the NAT of an Android emulator too. A lost datagram
// costs its frame; the player asks for a key frame and skips to it.
class UdpFrameOutput : public FrameOutput {
public:
    explicit UdpFrameOutput(StreamLog& log) : m_log(log) {}
    ~UdpFrameOutput() override { Stop(); }

    bool Start(in_addr address, uint16_t port, std::wstring& error) override;
    void Stop() override;
    uint16_t Port() const override { return m_port; }
    int Viewers() const override { return m_viewers; }
    bool TakeKeyFrameRequest() override { return m_keyFrameWanted.exchange(false); }
    void Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
              uint64_t now) override;
    std::wstring Url(const std::wstring& host) const override;

private:
    struct Player {
        sockaddr_in address;
        uint64_t lastHeardMs = 0;
        float loss = 0.0f;  // from its last hello
        bool lossReported = true;
    };

    void ReceiveLoop();
    void Hear(const sockaddr_in& from, const FrameHeader& header, uint64_t now);

    StreamLog& m_log;
    net::Socket m_socket;
    std::atomic<uint16_t> m_port{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<int> m_viewers{0};
    std::atomic<bool> m_keyFrameWanted{false};
    std::thread m_receiveThread;

    std::mutex m_playersMutex;
    std::map<uint64_t, Player> m_players;  // by address and port

    // The encode thread's.
    std::unique_ptr<PacketPacer> m_pacer;
    uint32_t m_frameNumber = 0;
    std::vector<std::string> m_datagrams;
};

}  // namespace gazeora
