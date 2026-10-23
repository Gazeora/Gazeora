#include "gazeora/host/stream/video_pipeline.h"

#include <algorithm>
#include <random>
#include <utility>

#include "gazeora/host/encode/color_convert.h"
#include "gazeora/host/stream/stream_view_info.h"

namespace gazeora {

VideoPipeline::VideoPipeline(EncoderFactory factory)
    : m_factory(std::move(factory)),
      m_format(&RtpPayloadFormat::For(VideoCodec::H264)),
      m_parameterSets(*m_format),
      m_seq(uint16_t(std::random_device()())) {}

bool VideoPipeline::Prepare(const Config& config, uint32_t width, uint32_t height,
                            bool& reopened, std::wstring& error) {
    reopened = false;
    if (!m_encoder) {
        m_encoder = m_factory();
    }
    if (!m_encoder->Matches(config.codec, width, height)) {
        VideoEncoder::Settings settings;
        settings.codec = config.codec;
        settings.width = width;
        settings.height = height;
        settings.fps = config.fps;
        settings.bitrate = config.bitrate;
        settings.keyInterval = config.keyInterval;
        if (!m_encoder->Open(settings, error)) {
            return false;
        }
        m_format = &RtpPayloadFormat::For(config.codec);
        m_parameterSets = ParameterSetCache(*m_format);
        reopened = true;
    } else if (config.bitrate != m_config.bitrate) {
        m_encoder->SetBitrate(config.bitrate);
    }
    m_config = config;
    return true;
}

void VideoPipeline::RequestKeyFrame() {
    if (m_encoder) {
        m_encoder->RequestKeyFrame();
    }
}

bool VideoPipeline::Encode(const Picture& picture, const FrameSink& sink, bool& fmtpChanged,
                           std::wstring& error) {
    fmtpChanged = false;
    const VideoEncoder::Settings& size = m_encoder->Current();
    BgraToNv12(picture.bgra.data(), picture.width, size.width, size.height, m_nv12);
    m_encoded.clear();
    // Sample times in 100 ns units, from the 90 kHz capture clock.
    if (!m_encoder->Encode(m_nv12.data(), int64_t(picture.clock * 1000 / 9), m_encoded, error)) {
        m_encoder->Close();
        return false;
    }
    for (const VideoEncoder::Frame& frame : m_encoded) {
        SplitAnnexB(frame.annexB, m_nalUnits);
        // From the NAL units rather than the encoder's clean-point flag: an IDR (IRAP for
        // H.265) is what a player can start decoding from.
        bool keyFrame = std::any_of(m_nalUnits.begin(), m_nalUnits.end(),
                                    [&](const std::string& nal) { return m_format->IsKeyFrame(nal); });
        fmtpChanged |= m_parameterSets.Update(m_nalUnits);
        // Every key frame carries the parameter sets, for players that join on it.
        if (keyFrame) {
            m_parameterSets.PrependTo(m_nalUnits);
        }
        // How to show it, just before the picture itself.
        if (!m_viewInfo.empty()) {
            auto picture = std::find_if(
                m_nalUnits.begin(), m_nalUnits.end(),
                [&](const std::string& nal) { return IsPictureNal(m_config.codec, nal); });
            m_nalUnits.insert(picture, ViewInfoNal(m_config.codec, m_viewInfo));
        }
        // Back to the 90 kHz clock, rounding up so it lands on the captured value.
        uint32_t timestamp = uint32_t((uint64_t(frame.time) * 9 + 999) / 1000);
        m_format->Packetize(m_nalUnits, timestamp, m_config.ssrc, m_config.maxPacketSize, m_seq,
                            m_packets);
        m_annexB.clear();
        for (const std::string& nal : m_nalUnits) {
            m_annexB.append("\0\0\0\1", 4);
            m_annexB += nal;
        }
        sink(EncodedFrame{m_packets, m_annexB, timestamp, keyFrame, m_config.codec, size.width,
                          size.height});
    }
    return true;
}

void VideoPipeline::Close() {
    if (m_encoder) {
        m_encoder->Close();
    }
}

}  // namespace gazeora
