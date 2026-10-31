// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/camera_status_message.h"
#include "gazeora/host/stream/camera_status_monitor.h"
#include "gazeora/host/stream/head_pose_message.h"
#include "gazeora/host/stream/pointer_gate.h"
#include "gazeora/host/stream/pointer_message.h"
#include "gazeora/host/stream/remote_input_receiver.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gazeora {
namespace {

class RecordingSink final : public PointerEventSink {
public:
    void OnPointer(const PointerEvent& event) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
        m_heard.notify_all();
    }

    std::vector<PointerEvent> WaitFor(size_t count) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_heard.wait_for(lock, std::chrono::seconds(2), [&] { return m_events.size() >= count; });
        return m_events;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_heard;
    std::vector<PointerEvent> m_events;
};

class RecordingHeadSink final : public HeadPoseSink {
public:
    void OnHeadPose(const HeadPose& pose) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_poses.push_back(pose);
        m_heard.notify_all();
    }

    std::vector<HeadPose> WaitFor(size_t count) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_heard.wait_for(lock, std::chrono::seconds(2), [&] { return m_poses.size() >= count; });
        return m_poses;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_heard;
    std::vector<HeadPose> m_poses;
};

// A quarter turn to the left, about Y.
HeadPose QuarterTurnLeft(uint32_t sample) {
    HeadPose pose;
    pose.sample = sample;
    pose.y = std::sqrt(0.5f);
    pose.w = std::sqrt(0.5f);
    return pose;
}

PointerEvent Scroll(int vertical, int horizontal) {
    PointerEvent event;
    event.action = PointerAction::Scroll;
    event.wheelVertical = vertical;
    event.wheelHorizontal = horizontal;
    return event;
}

const uint8_t* Bytes(const std::string& text) {
    return reinterpret_cast<const uint8_t*>(text.data());
}

TEST(PointerMessageTest, RoundTripsThroughTheWire) {
    PointerEvent sent;
    sent.action = PointerAction::Press;
    sent.button = PointerButton::Right;
    sent.x = 0.25f;
    sent.y = 1.0f;
    std::string message = PointerMessage::Write(sent);
    ASSERT_EQ(message.size(), kFrameHeaderSize + kPointerPayloadSize);

    FrameHeader header;
    ASSERT_TRUE(header.Read(Bytes(message), message.size()));
    EXPECT_EQ(header.type, FrameMessage::Pointer);
    EXPECT_EQ(header.tail, kPointerPayloadSize);
    PointerEvent read;
    ASSERT_TRUE(PointerMessage::Read(Bytes(message) + kFrameHeaderSize, kPointerPayloadSize, read));
    EXPECT_EQ(read.action, PointerAction::Press);
    EXPECT_EQ(read.button, PointerButton::Right);
    EXPECT_NEAR(read.x, 0.25f, 1e-4f);
    EXPECT_FLOAT_EQ(read.y, 1.0f);
}

TEST(PointerMessageTest, KeepsTheWheelsSign) {
    std::string message = PointerMessage::Write(Scroll(-240, 120));
    PointerEvent read;
    ASSERT_TRUE(PointerMessage::Read(Bytes(message) + kFrameHeaderSize, kPointerPayloadSize, read));
    EXPECT_EQ(read.wheelVertical, -240);
    EXPECT_EQ(read.wheelHorizontal, 120);
}

TEST(PointerMessageTest, CarriesAClickWhole) {
    PointerEvent click;
    click.action = PointerAction::Click;
    click.x = 0.5f;
    std::string message = PointerMessage::Write(click);
    PointerEvent read;
    ASSERT_TRUE(PointerMessage::Read(Bytes(message) + kFrameHeaderSize, kPointerPayloadSize, read));
    EXPECT_EQ(read.action, PointerAction::Click);
    EXPECT_NEAR(read.x, 0.5f, 1e-4f);
}

TEST(PointerMessageTest, RefusesShortOrUnknownPayloads) {
    std::string message = PointerMessage::Write(Scroll(1, 0));
    std::string payload = message.substr(kFrameHeaderSize);
    PointerEvent read;
    EXPECT_FALSE(PointerMessage::Read(Bytes(payload), payload.size() - 1, read));
    payload[0] = char(99);
    EXPECT_FALSE(PointerMessage::Read(Bytes(payload), payload.size(), read));
}

TEST(PointerGateTest, PassesEventsOnOnceItHasATarget) {
    RecordingSink sink;
    PointerGate gate;
    gate.OnPointer(Scroll(1, 0));  // no target yet
    gate.SetTarget(&sink);
    gate.OnPointer(Scroll(2, 0));

    std::vector<PointerEvent> heard = sink.WaitFor(1);
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_EQ(heard[0].wheelVertical, 2);
}

TEST(HeadPoseMessageTest, RoundTripsThroughTheWire) {
    std::string message = HeadPoseMessage::Write(QuarterTurnLeft(0x01020304));
    ASSERT_EQ(message.size(), kFrameHeaderSize + kHeadPosePayloadSize);

    FrameHeader header;
    ASSERT_TRUE(header.Read(Bytes(message), message.size()));
    EXPECT_EQ(header.type, FrameMessage::HeadPose);
    EXPECT_EQ(header.tail, kHeadPosePayloadSize);
    HeadPose read;
    ASSERT_TRUE(
        HeadPoseMessage::Read(Bytes(message) + kFrameHeaderSize, kHeadPosePayloadSize, read));
    EXPECT_EQ(read.sample, 0x01020304u);
    EXPECT_FLOAT_EQ(read.x, 0.0f);
    EXPECT_NEAR(read.y, std::sqrt(0.5f), 1e-4f);
    EXPECT_FLOAT_EQ(read.z, 0.0f);
    EXPECT_NEAR(read.w, std::sqrt(0.5f), 1e-4f);
}

TEST(HeadPoseMessageTest, KeepsTheSigns) {
    HeadPose sent;
    sent.x = -0.5f;
    sent.y = 0.5f;
    sent.z = -0.5f;
    sent.w = -0.5f;
    std::string message = HeadPoseMessage::Write(sent);
    HeadPose read;
    ASSERT_TRUE(
        HeadPoseMessage::Read(Bytes(message) + kFrameHeaderSize, kHeadPosePayloadSize, read));
    EXPECT_NEAR(read.x, -0.5f, 1e-4f);
    EXPECT_NEAR(read.y, 0.5f, 1e-4f);
    EXPECT_NEAR(read.z, -0.5f, 1e-4f);
    EXPECT_NEAR(read.w, -0.5f, 1e-4f);
}

TEST(HeadPoseMessageTest, RefusesShortPayloadsAndNonUnitQuaternions) {
    HeadPose zero;
    zero.w = 0.0f;
    std::string message = HeadPoseMessage::Write(zero);
    HeadPose read;
    EXPECT_FALSE(
        HeadPoseMessage::Read(Bytes(message) + kFrameHeaderSize, kHeadPosePayloadSize, read));
    message = HeadPoseMessage::Write(QuarterTurnLeft(1));
    EXPECT_FALSE(
        HeadPoseMessage::Read(Bytes(message) + kFrameHeaderSize, kHeadPosePayloadSize - 1, read));
}

CameraReport DarkCamera() {
    CameraReport report;
    report.state = CameraState::Dark;
    report.brightness = 17;
    return report;
}

TEST(CameraStatusMessageTest, RoundTripsThroughTheWire) {
    std::string message = CameraStatusMessage::Write(DarkCamera());
    ASSERT_EQ(message.size(), kFrameHeaderSize + CameraStatusMessage::kPayloadSize);

    FrameHeader header;
    ASSERT_TRUE(header.Read(Bytes(message), message.size()));
    EXPECT_EQ(header.type, FrameMessage::CameraStatus);
    EXPECT_EQ(header.tail, CameraStatusMessage::kPayloadSize);
    CameraReport read;
    ASSERT_TRUE(CameraStatusMessage::Read(Bytes(message) + kFrameHeaderSize,
                                          CameraStatusMessage::kPayloadSize, read));
    EXPECT_EQ(read.state, CameraState::Dark);
    EXPECT_EQ(read.brightness, 17);
}

TEST(CameraStatusMessageTest, RefusesShortPayloadsAndUnknownStates) {
    CameraReport read;
    const uint8_t unknown[] = {kCameraStateCount, 0};
    EXPECT_FALSE(CameraStatusMessage::Read(unknown, sizeof(unknown), read));
    const uint8_t on[] = {uint8_t(CameraState::On), 0};
    EXPECT_FALSE(CameraStatusMessage::Read(on, 1, read));
    EXPECT_TRUE(CameraStatusMessage::Read(on, sizeof(on), read));
}

TEST(RemoteInputReceiverTest, HearsInputDatagramsFromAdmittedAddressesOnly) {
    net::WinsockRuntime winsock;
    std::wstring error;
    ASSERT_TRUE(winsock.Start(error));
    RecordingSink sink;
    RecordingHeadSink head;
    CameraStatusMonitor camera;
    StreamLog log;
    in_addr loopback;
    loopback.s_addr = htonl(INADDR_LOOPBACK);
    std::atomic<bool> watching{false};
    std::atomic<uint32_t> asked{0};
    RemoteInputReceiver receiver(sink, head, camera, log, [&](const in_addr& address) {
        asked = address.s_addr;
        return watching.load();
    });
    ASSERT_TRUE(receiver.Start(loopback, 0, error)) << Utf8::FromWide(error);

    net::Socket phone(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    sockaddr_in pc = {};
    pc.sin_family = AF_INET;
    pc.sin_addr = loopback;
    pc.sin_port = htons(receiver.Port());
    auto send = [&](const std::string& datagram) {
        sendto(phone.Get(), datagram.data(), int(datagram.size()), 0,
               reinterpret_cast<const sockaddr*>(&pc), sizeof(pc));
    };
    // Not watching: dropped.
    send(PointerMessage::Write(Scroll(5, 0)));
    send(CameraStatusMessage::Write(DarkCamera()));
    send(HeadPoseMessage::Write(QuarterTurnLeft(1)));
    EXPECT_TRUE(sink.WaitFor(1).empty());
    EXPECT_TRUE(head.WaitFor(1).empty());
    EXPECT_FALSE(camera.Reading().heard);
    EXPECT_EQ(asked.load(), loopback.s_addr);

    watching = true;
    send("not a pointer");
    send(PointerMessage::Write(Scroll(7, 0)));
    send(CameraStatusMessage::Write(DarkCamera()));  // before the pose, so heard by then
    send(HeadPoseMessage::Write(QuarterTurnLeft(2)));
    std::vector<PointerEvent> heard = sink.WaitFor(1);
    std::vector<HeadPose> poses = head.WaitFor(1);
    receiver.Stop();
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_EQ(heard[0].wheelVertical, 7);
    ASSERT_EQ(poses.size(), 1u);
    EXPECT_EQ(poses[0].sample, 2u);
    EXPECT_EQ(camera.Reading().report.state, CameraState::Dark);
}

}  // namespace
}  // namespace gazeora
