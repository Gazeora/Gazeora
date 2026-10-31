// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/pointer_gate.h"
#include "gazeora/host/stream/pointer_input_receiver.h"
#include "gazeora/host/stream/pointer_message.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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

TEST(PointerInputReceiverTest, HearsPointerDatagramsFromAdmittedAddressesOnly) {
    net::WinsockRuntime winsock;
    std::wstring error;
    ASSERT_TRUE(winsock.Start(error));
    RecordingSink sink;
    StreamLog log;
    in_addr loopback;
    loopback.s_addr = htonl(INADDR_LOOPBACK);
    std::atomic<bool> watching{false};
    std::atomic<uint32_t> asked{0};
    PointerInputReceiver receiver(sink, log, [&](const in_addr& address) {
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
    EXPECT_TRUE(sink.WaitFor(1).empty());
    EXPECT_EQ(asked.load(), loopback.s_addr);

    watching = true;
    send("not a pointer");
    send(PointerMessage::Write(Scroll(7, 0)));
    std::vector<PointerEvent> heard = sink.WaitFor(1);
    receiver.Stop();
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_EQ(heard[0].wheelVertical, 7);
}

}  // namespace
}  // namespace gazeora
