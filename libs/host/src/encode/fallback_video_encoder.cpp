// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/fallback_video_encoder.h"

#include <cstring>

namespace gazeora {

namespace {

constexpr wchar_t kErrorSeparator[] = L"; ";

std::wstring NoEncoderError(const VideoCodec& codec) {
    const std::string& id = codec.Id();  // ASCII
    return L"no encoder for " + std::wstring(id.begin(), id.end());
}

}  // namespace

FallbackVideoEncoder::FallbackVideoEncoder(
    std::vector<std::shared_ptr<const VideoEncoderProvider>> providers)
    : m_providers(std::move(providers)) {}

bool FallbackVideoEncoder::Open(const Settings& settings, std::wstring& error) {
    Close();
    std::wstring errors;
    for (const auto& provider : m_providers) {
        if (provider->Supports(settings.codec) && TryOpen(*provider, settings, errors)) {
            return true;
        }
    }
    error = errors.empty() ? NoEncoderError(settings.codec) : errors;
    return false;
}

bool FallbackVideoEncoder::TryOpen(const VideoEncoderProvider& provider, const Settings& settings,
                                   std::wstring& errors) {
    std::unique_ptr<VideoEncoder> encoder = provider.Create();
    std::wstring error;
    if (encoder->Open(settings, error)) {
        m_active = std::move(encoder);
        return true;
    }
    if (!errors.empty()) {
        errors += kErrorSeparator;
    }
    errors += error;
    return false;
}

void FallbackVideoEncoder::Close() {
    if (m_active) {
        m_active->Close();
    }
}

bool FallbackVideoEncoder::IsOpen() const { return m_active && m_active->IsOpen(); }

const VideoEncoder::Settings& FallbackVideoEncoder::Current() const {
    return m_active ? m_active->Current() : m_noSettings;
}

const std::wstring& FallbackVideoEncoder::Name() const {
    return m_active ? m_active->Name() : m_noName;
}

bool FallbackVideoEncoder::IsHardware() const { return m_active && m_active->IsHardware(); }

void FallbackVideoEncoder::SetBitrate(uint32_t bitrate) {
    if (m_active) {
        m_active->SetBitrate(bitrate);
    }
}

void FallbackVideoEncoder::RequestKeyFrame() {
    if (m_active) {
        m_active->RequestKeyFrame();
    }
}

bool FallbackVideoEncoder::Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                                  std::wstring& error) {
    return IsOpen() ? m_active->Encode(nv12, time, out, error) : RefuseClosed(error);
}

bool FallbackVideoEncoder::EncodeTexture(ID3D11Texture2D* nv12, int64_t time,
                                         std::vector<Frame>& out, std::wstring& error) {
    return IsOpen() ? m_active->EncodeTexture(nv12, time, out, error) : RefuseClosed(error);
}

bool FallbackVideoEncoder::RefuseClosed(std::wstring& error) {
    error = L"encoder not open";
    return false;
}

}  // namespace gazeora
