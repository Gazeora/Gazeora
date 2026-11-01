// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/encode/video_encoder_provider.h"

namespace gazeora {

// Encoders through Media Foundation, with the GPU's encoder when there is one. They need COM
// (multithreaded) and MFStartup on the thread that uses them. H.264 is always supported:
// Microsoft's software encoder ships with Windows.
class MediaFoundationEncoderProvider final : public VideoEncoderProvider {
public:
    const std::wstring& Name() const override { return m_name; }
    bool Supports(VideoCodec codec) const override;
    std::unique_ptr<VideoEncoder> Create() const override;

private:
    const std::wstring m_name = L"Media Foundation";
};

}  // namespace gazeora
