// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/host/encode/video_encoder.h"

namespace gazeora {

// Null Object pattern: stands in for an encoder a plugin couldn't create, so the next
// provider gets its turn.
class RefusedVideoEncoder final : public VideoEncoder {
public:
    RefusedVideoEncoder(std::wstring name, std::wstring reason)
        : m_name(std::move(name)), m_reason(std::move(reason)) {}

    bool Open(const Settings&, std::wstring& error) override { return Refuse(error); }
    void Close() override {}
    bool IsOpen() const override { return false; }
    const Settings& Current() const override { return m_settings; }
    const std::wstring& Name() const override { return m_name; }
    bool IsHardware() const override { return false; }
    void SetBitrate(uint32_t) override {}
    void RequestKeyFrame() override {}
    bool Encode(const uint8_t*, int64_t, std::vector<Frame>&, std::wstring& error) override {
        return Refuse(error);
    }

private:
    bool Refuse(std::wstring& error) const {
        error = m_name + L" " + m_reason;
        return false;
    }

    const std::wstring m_name;
    const std::wstring m_reason;
    const Settings m_settings{};
};

}  // namespace gazeora
