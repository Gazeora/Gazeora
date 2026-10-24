#include "gazeora/host/stream/packet_send_queue.h"

#include <gtest/gtest.h>
#include <ws2tcpip.h>

#include <string>
#include <vector>

#include "gazeora/host/stream/net.h"

namespace gazeora {
namespace {

class PacketSendQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::wstring error;
        ASSERT_TRUE(winsock.Start(error));
        in_addr loopback;
        loopback.s_addr = htonl(INADDR_LOOPBACK);
        uint16_t port = 0;
        receiver = net::BindUdp(loopback, 0, port);
        uint16_t unused = 0;
        sender = net::BindUdp(loopback, 0, unused);
        ASSERT_TRUE(receiver && sender);
        DWORD timeoutMs = 1000;
        setsockopt(receiver.Get(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeoutMs),
                   sizeof(timeoutMs));
        int buffer = 8 << 20;
        setsockopt(receiver.Get(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buffer),
                   sizeof(buffer));
        target.sin_family = AF_INET;
        target.sin_addr = loopback;
        target.sin_port = htons(port);
    }

    // `count` packets, each its index as text.
    static std::vector<std::string> Numbered(size_t count, size_t from = 0) {
        std::vector<std::string> packets;
        for (size_t i = from; i < from + count; ++i) packets.push_back(std::to_string(i));
        return packets;
    }

    std::vector<std::string> Receive(size_t count) {
        std::vector<std::string> got;
        char data[2048];
        while (got.size() < count) {
            int length = recv(receiver.Get(), data, sizeof(data), 0);
            if (length <= 0) break;
            got.emplace_back(data, size_t(length));
        }
        return got;
    }

    static double Now() {
        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        return double(now.QuadPart) / double(frequency.QuadPart);
    }

    net::WinsockRuntime winsock;
    net::Socket receiver;
    net::Socket sender;
    sockaddr_in target = {};
};

TEST_F(PacketSendQueueTest, SendsEveryPacketInOrder) {
    PacketSendQueue queue;
    queue.Enqueue(sender.Get(), {target}, Numbered(20), PacingPlan::For(20, 20 * 1400, 0, 30));
    queue.Enqueue(sender.Get(), {target}, Numbered(20, 20), PacingPlan::AtOnce(20));
    queue.Flush();
    EXPECT_EQ(Receive(40), Numbered(40));
}

TEST_F(PacketSendQueueTest, ReturnsAtOnceAndSpreadsThePackets) {
    PacketSendQueue queue;
    // 5 bursts of 4: 4 ms from the first to the last.
    PacingPlan plan = PacingPlan::For(20, 20 * 1400, 2000000, 30);
    ASSERT_EQ(plan.Bursts(), 5u);
    double start = Now();
    queue.Enqueue(sender.Get(), {target}, Numbered(20), plan);
    double queued = Now() - start;
    queue.Flush();
    double sent = Now() - start;
    EXPECT_LT(queued, 0.002) << "the encode thread doesn't wait for the pacing";
    EXPECT_GE(sent, 0.0035);
    EXPECT_LT(sent, 0.020) << "a late timer doesn't add up over the frame";
}

TEST_F(PacketSendQueueTest, StillPacesAFrameWithAReportBehindIt) {
    PacketSendQueue queue;
    PacingPlan plan = PacingPlan::For(20, 20 * 1400, 2000000, 30);
    double start = Now();
    queue.Enqueue(sender.Get(), {target}, Numbered(20), plan);
    queue.Enqueue(sender.Get(), {target}, Numbered(1, 20), PacingPlan::AtOnce(1));
    queue.Flush();
    EXPECT_GE(Now() - start, 0.0035);
    EXPECT_EQ(Receive(21), Numbered(21));
}

TEST_F(PacketSendQueueTest, CatchesUpWithoutWaitsWhenFramesQueueUp) {
    PacketSendQueue queue;
    // Each frame alone takes 8 ms; queued together the first has a frame behind it.
    PacingPlan plan = PacingPlan::For(36, 36 * 1400, 2000000, 30);
    ASSERT_GE(plan.SpreadUs(), 8000);
    double start = Now();
    queue.Enqueue(sender.Get(), {target}, Numbered(36), plan);
    queue.Enqueue(sender.Get(), {target}, Numbered(36, 36), plan);
    queue.Flush();
    EXPECT_LT(Now() - start, 2 * plan.SpreadUs() / 1e6);
    EXPECT_EQ(Receive(72), Numbered(72));
}

}  // namespace
}  // namespace gazeora
