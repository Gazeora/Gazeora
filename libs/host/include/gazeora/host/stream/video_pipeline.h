#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/nv12_readback.h"
#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/host/stream/frame_mailbox.h"
#include "gazeora/host/stream/rtp_video.h"

namespace gazeora {

// Pictures in, encoded frames out: NV12 conversion, encoding, parameter sets in front of
// every key frame, RTP packetization. A picture already on the GPU as NV12 goes straight to a
// GPU encoder that takes its device's textures, or is read back for one that doesn't. The encoder
// comes from a factory, so tests (and other encoder back ends) can supply their own. Use from one
// thread.
class VideoPipeline {
public:
    using EncoderFactory = std::function<std::unique_ptr<VideoEncoder>()>;

    // One encoded frame, both as RTP packets (RTSP) and as its access unit (everything else).
    struct EncodedFrame {
        const std::vector<std::string>& packets;
        const std::string& annexB;  // NAL units with start codes, parameter sets included
        uint32_t timestamp;         // 90 kHz
        bool keyFrame;              // a player can start decoding from it
        VideoCodec codec;
        uint32_t width;
        uint32_t height;
        uint32_t bitrate;  // the stream's, bits/s, for pacing its packets (PacingPlan)
        uint32_t fps;
    };
    using FrameSink = std::function<void(const EncodedFrame& frame)>;

    struct Config {
        VideoCodec codec = VideoCodec::H264;
        uint32_t fps = 30;
        uint32_t bitrate = 0;
        uint32_t keyInterval = 30;
        uint32_t ssrc = 0;
        size_t maxPacketSize = 1400;
    };

    explicit VideoPipeline(EncoderFactory factory = CreateVideoEncoder);

    // Opens (or reopens) the encoder when the codec, size, frame rate or device changed, and
    // applies a new bitrate. `device` is the one the pictures' textures are on, null for
    // pictures in memory. `reopened` tells whether a new encoder was opened. False with `error` set
    // if no encoder could be opened.
    bool Prepare(const Config& config, uint32_t width, uint32_t height, bool& reopened,
                 std::wstring& error, ID3D11Device* device = nullptr);
    void RequestKeyFrame();
    // The SEI message frames carry from now on (stream_view_info.h); empty for none.
    void SetViewInfo(std::string text) { m_viewInfo = std::move(text); }
    // Encodes `picture` and hands each finished frame to `sink`. `fmtpChanged` is set when
    // the parameter sets changed, so the SDP should be updated (Fmtp()). On failure the
    // encoder is closed and reopened by the next Prepare.
    bool Encode(const Picture& picture, const FrameSink& sink, bool& fmtpChanged,
                std::wstring& error);
    void Close();

    bool IsOpen() const { return m_encoder && m_encoder->IsOpen(); }
    VideoCodec Codec() const { return m_config.codec; }
    std::wstring EncoderName() const { return m_encoder ? m_encoder->Name() : std::wstring(); }
    std::string Fmtp() const { return m_parameterSets.Fmtp(); }

private:
    bool Open(const Config& config, uint32_t width, uint32_t height, ID3D11Device* device,
              std::wstring& error);
    // The picture as NV12 in m_nv12, for an encoder that takes it from memory.
    bool ToMemory(const Picture& picture, std::wstring& error);

    EncoderFactory m_factory;
    std::unique_ptr<VideoEncoder> m_encoder;
    Config m_config;
    const RtpPayloadFormat* m_format;
    ParameterSetCache m_parameterSets;
    uint16_t m_seq;
    // Reused between frames, so steady streaming doesn't allocate.
    std::vector<uint8_t> m_nv12;
    Nv12Readback m_readback;
    // The device and codec no encoder took textures for: their pictures are read back.
    Microsoft::WRL::ComPtr<ID3D11Device> m_refusedDevice;
    VideoCodec m_refusedCodec = VideoCodec::H264;
    std::vector<VideoEncoder::Frame> m_encoded;
    std::vector<std::string> m_nalUnits;
    std::vector<std::string> m_packets;
    std::string m_annexB;
    std::string m_viewInfo;
};

}  // namespace gazeora
