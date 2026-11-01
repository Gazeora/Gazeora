// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_output_factory.h"

namespace gazeora {

// What the recording output answers and does (exported_output_test.cpp).
inline constexpr uint16_t kPort = 4242;
inline constexpr uint16_t kRefusedPort = 1;
inline constexpr uint64_t kPlayer = 9;
inline constexpr float kLoss = 0.25f;
inline constexpr float kTouchX = 0.5f;
inline constexpr float kTouchY = 0.25f;
inline const StreamProtocol kRecording("recording");

// What the recording output saw. The test drives the output from its own thread, one call at a
// time, so nothing here needs a lock.
struct Record {
    uint32_t startedOn = 0;
    uint16_t askedPort = 0;
    bool stopped = false;
    std::string codec;
    uint8_t flags = 0;
    uint8_t tsStreamType = 0;
    std::string annexB;
};

inline Record& TheRecord() {
    static Record record;
    return record;
}

// A C++ output, as a bundled plugin has one: records what reaches it, and plays a phone that
// touches the screen over the output's own link as soon as it starts.
class RecordingOutput final : public FrameOutput {
public:
    explicit RecordingOutput(StreamLog& log) : m_log(log) {}

    bool Start(in_addr address, uint16_t port, std::wstring& error) override {
        TheRecord() = {address.s_addr, port};
        m_log.Write("recording on %u", unsigned(port));
        if (m_pointer) {
            PointerEvent touch;
            touch.action = PointerAction::Press;
            touch.x = kTouchX;
            touch.y = kTouchY;
            m_pointer->OnPointer(touch);
        }
        error = port == kRefusedPort ? L"port taken" : L"";
        return port != kRefusedPort;
    }
    void Stop() override { TheRecord().stopped = true; }
    uint16_t Port() const override { return kPort; }
    int Viewers() const override { return 2; }
    int AppViewers() const override { return 1; }
    bool TakeKeyFrameRequest() override { return true; }
    void Send(const VideoPipeline::EncodedFrame& frame, LossReportSink& loss,
              uint64_t now) override {
        TheRecord().codec = frame.codec.Id();
        TheRecord().flags = frame.format.FrameFlags();
        TheRecord().tsStreamType = frame.format.TsStreamType();
        TheRecord().annexB = frame.annexB;
        loss.Report(kPlayer, kLoss, now);
    }
    std::wstring Url(const std::wstring& host) const override { return L"rec://" + host; }
    std::wstring Status() const override { return L"recording"; }
    bool NeedsUsbDriver() const override { return true; }
    std::vector<UsbPhone> PhonesNeedingDriver() const override {
        return {{L"USB\\VID_1234&PID_5678", L"Test phone"}};
    }
    void SetPointerSink(PointerEventSink* sink) override { m_pointer = sink; }
    bool Watching(const in_addr& address) override {
        return address.s_addr == htonl(INADDR_LOOPBACK);
    }

private:
    StreamLog& m_log;
    PointerEventSink* m_pointer = nullptr;
};

class RecordingFactory final : public FrameOutputFactory {
public:
    StreamProtocol Protocol() const override { return kRecording; }
    std::unique_ptr<FrameOutput> Create(StreamLog& log) const override {
        return std::make_unique<RecordingOutput>(log);
    }
};

}  // namespace gazeora
