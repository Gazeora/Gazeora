// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/video_encoder_registry.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gazeora/host/encode/media_foundation_encoder_provider.h"

namespace gazeora {
namespace {

// Opens or refuses as told, and encodes nothing.
class FakeEncoder final : public VideoEncoder {
public:
    FakeEncoder(std::wstring name, bool opens) : m_name(std::move(name)), m_opens(opens) {}

    bool Open(const Settings& settings, std::wstring& error) override {
        m_settings = settings;
        m_open = m_opens;
        if (!m_opens) {
            error = m_name + L" refused";
        }
        return m_opens;
    }
    void Close() override { m_open = false; }
    bool IsOpen() const override { return m_open; }
    const Settings& Current() const override { return m_settings; }
    const std::wstring& Name() const override { return m_name; }
    bool IsHardware() const override { return false; }
    void SetBitrate(uint32_t) override {}
    void RequestKeyFrame() override {}
    bool Encode(const uint8_t*, int64_t, std::vector<Frame>&, std::wstring&) override {
        return true;
    }

private:
    const std::wstring m_name;
    const bool m_opens;
    bool m_open = false;
    Settings m_settings;
};

class FakeProvider final : public VideoEncoderProvider {
public:
    FakeProvider(std::wstring name, bool supportsH265, bool opens)
        : m_name(std::move(name)), m_supportsH265(supportsH265), m_opens(opens) {}

    const std::wstring& Name() const override { return m_name; }
    bool Supports(VideoCodec codec) const override {
        return codec == VideoCodec::H264 || m_supportsH265;
    }
    std::unique_ptr<VideoEncoder> Create() const override {
        return std::make_unique<FakeEncoder>(m_name, m_opens);
    }

private:
    const std::wstring m_name;
    const bool m_supportsH265;
    const bool m_opens;
};

VideoEncoder::Settings SettingsFor(VideoCodec codec) {
    VideoEncoder::Settings settings;
    settings.codec = codec;
    settings.width = 320;
    settings.height = 240;
    return settings;
}

TEST(VideoEncoderRegistryTest, AvailableWhenAnyProviderSupportsTheCodec) {
    VideoEncoderRegistry registry;
    EXPECT_FALSE(registry.Available(VideoCodec::H264));
    registry.Register(std::make_shared<FakeProvider>(L"a", false, true));
    EXPECT_TRUE(registry.Available(VideoCodec::H264));
    EXPECT_FALSE(registry.Available(VideoCodec::H265));
    registry.Register(std::make_shared<FakeProvider>(L"b", true, true));
    EXPECT_TRUE(registry.Available(VideoCodec::H265));
}

TEST(VideoEncoderRegistryTest, OpensWithTheFirstProviderThatManages) {
    VideoEncoderRegistry registry;
    registry.Register(std::make_shared<FakeProvider>(L"no h265", false, true));
    registry.Register(std::make_shared<FakeProvider>(L"refuses", true, false));
    registry.Register(std::make_shared<FakeProvider>(L"opens", true, true));
    std::unique_ptr<VideoEncoder> encoder = registry.Create();
    EXPECT_FALSE(encoder->IsOpen());
    EXPECT_TRUE(encoder->Name().empty());

    std::wstring error;
    ASSERT_TRUE(encoder->Open(SettingsFor(VideoCodec::H265), error)) << error;
    EXPECT_EQ(encoder->Name(), L"opens");
    EXPECT_TRUE(encoder->Matches(VideoCodec::H265, 320, 240));

    ASSERT_TRUE(encoder->Open(SettingsFor(VideoCodec::H264), error)) << error;
    EXPECT_EQ(encoder->Name(), L"no h265");  // first in line for H.264
}

TEST(VideoEncoderRegistryTest, ReportsWhyEveryProviderFailed) {
    VideoEncoderRegistry registry;
    registry.Register(std::make_shared<FakeProvider>(L"a", false, false));
    registry.Register(std::make_shared<FakeProvider>(L"b", false, false));
    std::wstring error;
    EXPECT_FALSE(registry.Create()->Open(SettingsFor(VideoCodec::H264), error));
    EXPECT_EQ(error, L"a refused; b refused");
    EXPECT_FALSE(registry.Create()->Open(SettingsFor(VideoCodec::H265), error));
    EXPECT_EQ(error, L"no encoder for h265");
}

TEST(VideoEncoderRegistryTest, ClosedEncoderRefusesFrames) {
    VideoEncoderRegistry registry;
    std::unique_ptr<VideoEncoder> encoder = registry.Create();
    std::vector<VideoEncoder::Frame> out;
    std::wstring error;
    EXPECT_FALSE(encoder->Encode(nullptr, 0, out, error));
    EXPECT_FALSE(error.empty());
}

TEST(VideoEncoderRegistryTest, MediaFoundationOffersOnlyH264AndH265) {
    EXPECT_TRUE(MediaFoundationEncoderProvider().Supports(VideoCodec::H264));
    EXPECT_FALSE(MediaFoundationEncoderProvider().Supports(VideoCodec("av1")));
}

TEST(VideoEncoderRegistryTest, RefusesNull) {
    VideoEncoderRegistry registry;
    EXPECT_THROW(registry.Register(nullptr), std::invalid_argument);
}

}  // namespace
}  // namespace gazeora
