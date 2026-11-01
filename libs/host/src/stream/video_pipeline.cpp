// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/video_pipeline.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

#include "gazeora/host/encode/color_convert.h"
#include "gazeora/host/stream/stream_view_info.h"

namespace gazeora {

namespace {

// H.264's format in `codecs`, which every stream falls back to.
const VideoCodecFormat& H264FormatIn(const std::shared_ptr<const VideoCodecCatalog>& codecs) {
    const VideoCodecInfo* info = codecs ? codecs->Find(VideoCodec::H264) : nullptr;
    if (!info || !info->format) {
        throw std::invalid_argument("VideoPipeline: the codecs have no H.264 format");
    }
    return *info->format;
}

}  // namespace

VideoPipeline::VideoPipeline(EncoderFactory factory,
                             std::shared_ptr<const VideoCodecCatalog> codecs)
    : m_factory(std::move(factory)),
      m_codecs(std::move(codecs)),
      m_format(&H264FormatIn(m_codecs)),
      m_parameterSets(m_format->Rtp()),
      m_seq(uint16_t(std::random_device()())) {}

const VideoCodecFormat* VideoPipeline::FormatOf(const VideoCodec& codec) const {
    const VideoCodecInfo* info = m_codecs->Find(codec);
    return info ? info->format.get() : nullptr;
}

bool VideoPipeline::Prepare(const Config& config, uint32_t width, uint32_t height, bool& reopened,
                            std::wstring& error, ID3D11Device* device) {
    reopened = false;
    if (!m_encoder) {
        m_encoder = m_factory();
    }
    bool refused = device == m_refusedDevice.Get() && config.codec == m_refusedCodec;
    ID3D11Device* wanted = refused ? nullptr : device;
    const VideoEncoder::Settings& current = m_encoder->Current();
    bool sameRate = current.fps == config.fps && current.keyInterval == config.keyInterval;
    if (!sameRate || !m_encoder->Matches(config.codec, width, height, wanted)) {
        if (!Open(config, width, height, wanted, error)) {
            if (!wanted) {
                return false;
            }
            // No encoder takes this device's textures: read them back from now on.
            m_refusedDevice = device;
            m_refusedCodec = config.codec;
            error.clear();
            if (!Open(config, width, height, nullptr, error)) {
                return false;
            }
        }
        reopened = true;
    } else if (config.bitrate != m_config.bitrate) {
        m_encoder->SetBitrate(config.bitrate);
    }
    m_config = config;
    return true;
}

bool VideoPipeline::Open(const Config& config, uint32_t width, uint32_t height,
                         ID3D11Device* device, std::wstring& error) {
    const VideoCodecFormat* format = FormatOf(config.codec);
    if (!format) {
        error = L"no stream format for the codec " +
                std::wstring(config.codec.Id().begin(), config.codec.Id().end());
        return false;
    }
    VideoEncoder::Settings settings;
    settings.codec = config.codec;
    settings.width = width;
    settings.height = height;
    settings.fps = config.fps;
    settings.bitrate = config.bitrate;
    settings.keyInterval = config.keyInterval;
    settings.device = device;
    if (!m_encoder->Open(settings, error)) {
        return false;
    }
    m_format = format;
    m_parameterSets = ParameterSetCache(format->Rtp());
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
    m_encoded.clear();
    // Sample times in 100 ns units, from the 90 kHz capture clock.
    const int64_t time = int64_t(picture.clock * 1000 / 9);
    if (picture.nv12 && size.device) {
        if (!m_encoder->EncodeTexture(picture.nv12.Get(), time, m_encoded, error)) {
            m_encoder->Close();
            return false;
        }
    } else if (!ToMemory(picture, error)) {
        return false;
    } else if (!m_encoder->Encode(m_nv12.data(), time, m_encoded, error)) {
        m_encoder->Close();
        return false;
    }
    for (const VideoEncoder::Frame& frame : m_encoded) {
        SplitAnnexB(frame.annexB, m_nalUnits);
        // From the NAL units rather than the encoder's clean-point flag: an IDR (IRAP for
        // H.265) is what a player can start decoding from.
        bool keyFrame =
            std::any_of(m_nalUnits.begin(), m_nalUnits.end(),
                        [&](const std::string& nal) { return m_format->Rtp().IsKeyFrame(nal); });
        fmtpChanged |= m_parameterSets.Update(m_nalUnits);
        // Every key frame carries the parameter sets, for players that join on it.
        if (keyFrame) {
            m_parameterSets.PrependTo(m_nalUnits);
        }
        // How to show it, just before the picture itself, for a codec that can carry it.
        if (!m_viewInfo.empty() && !m_format->SeiHeader().empty()) {
            auto picture =
                std::find_if(m_nalUnits.begin(), m_nalUnits.end(),
                             [&](const std::string& nal) { return m_format->IsPictureNal(nal); });
            m_nalUnits.insert(picture, ViewInfoNal(*m_format, m_viewInfo));
        }
        // Back to the 90 kHz clock, rounding up so it lands on the captured value.
        uint32_t timestamp = uint32_t((uint64_t(frame.time) * 9 + 999) / 1000);
        m_format->Rtp().Packetize(m_nalUnits, timestamp, m_config.ssrc, m_config.maxPacketSize,
                                  m_seq, m_packets);
        m_annexB.clear();
        for (const std::string& nal : m_nalUnits) {
            m_annexB.append("\0\0\0\1", 4);
            m_annexB += nal;
        }
        sink(EncodedFrame{m_packets, m_annexB, timestamp, keyFrame, m_config.codec, *m_format,
                          size.width, size.height, m_config.bitrate, m_config.fps});
    }
    return true;
}

bool VideoPipeline::ToMemory(const Picture& picture, std::wstring& error) {
    const VideoEncoder::Settings& size = m_encoder->Current();
    if (picture.nv12) {
        return m_readback.Read(picture.nv12.Get(), size.width, size.height, m_nv12, error);
    }
    BgraToNv12(picture.bgra.data(), picture.width, size.width, size.height, m_nv12);
    return true;
}

void VideoPipeline::Close() {
    if (m_encoder) {
        m_encoder->Close();
    }
}

}  // namespace gazeora
