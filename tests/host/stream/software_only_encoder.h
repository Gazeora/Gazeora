#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/video_encoder.h"

namespace gazeora {

// Decorator pattern: Gazeora's encoder with the GPU encoders skipped. Opening a GPU encoder
// can take seconds (a laptop's GPU waking up), which would make the stream tests depend on
// the machine; the GPU encoders have their own tests (video_encoder_test.cpp).
class SoftwareOnlyEncoder final : public VideoEncoder {
public:
    explicit SoftwareOnlyEncoder(std::unique_ptr<VideoEncoder> inner) : m_inner(std::move(inner)) {}

    static std::unique_ptr<VideoEncoder> Create() {
        return std::make_unique<SoftwareOnlyEncoder>(CreateVideoEncoder());
    }

    bool Open(const Settings& settings, std::wstring& error) override {
        Settings software = settings;
        software.preferHardware = false;
        return m_inner->Open(software, error);
    }

    void Close() override { m_inner->Close(); }
    bool IsOpen() const override { return m_inner->IsOpen(); }
    const Settings& Current() const override { return m_inner->Current(); }
    const std::wstring& Name() const override { return m_inner->Name(); }
    bool IsHardware() const override { return m_inner->IsHardware(); }
    void SetBitrate(uint32_t bitrate) override { m_inner->SetBitrate(bitrate); }
    void RequestKeyFrame() override { m_inner->RequestKeyFrame(); }

    bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                std::wstring& error) override {
        return m_inner->Encode(nv12, time, out, error);
    }

private:
    std::unique_ptr<VideoEncoder> m_inner;
};

}  // namespace gazeora
