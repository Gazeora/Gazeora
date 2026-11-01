// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/frame_output_registry.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "gazeora/host/stream/built_in_stream_protocols.h"
#include "gazeora/host/stream/bundled_protocols.h"
#include "gazeora/host/stream/frame_stream.h"
#include "host/stream/software_only_encoder_provider.h"

namespace gazeora {
namespace {

constexpr uint16_t kFakePort = 4242;

// Starts without opening anything and takes no players.
class FakeOutput final : public FrameOutput {
public:
    bool Start(in_addr, uint16_t, std::wstring&) override { return true; }
    void Stop() override {}
    uint16_t Port() const override { return kFakePort; }
    int Viewers() const override { return 0; }
    bool TakeKeyFrameRequest() override { return false; }
    void Send(const VideoPipeline::EncodedFrame&, LossReportSink&, uint64_t) override {}
    std::wstring Url(const std::wstring& host) const override { return L"fake://" + host; }
};

class FakeOutputFactory final : public FrameOutputFactory {
public:
    explicit FakeOutputFactory(StreamProtocol protocol) : m_protocol(protocol) {}

    StreamProtocol Protocol() const override { return m_protocol; }
    std::unique_ptr<FrameOutput> Create(StreamLog&) const override {
        ++m_created;
        return std::make_unique<FakeOutput>();
    }
    int Created() const { return m_created; }

private:
    const StreamProtocol m_protocol;
    mutable int m_created = 0;
};

TEST(FrameOutputRegistryTest, CreatesFromTheFactoryForTheProtocol) {
    FrameOutputRegistry registry;
    auto factory = std::make_shared<FakeOutputFactory>(StreamProtocol::Udp);
    registry.Register(factory);
    StreamLog log;
    EXPECT_TRUE(registry.Has(StreamProtocol::Udp));
    EXPECT_NE(registry.Create(StreamProtocol::Udp, log), nullptr);
    EXPECT_EQ(factory->Created(), 1);
    EXPECT_FALSE(registry.Has(StreamProtocol::Http));
    EXPECT_EQ(registry.Create(StreamProtocol::Http, log), nullptr);
}

TEST(FrameOutputRegistryTest, LaterFactoryReplacesEarlierOne) {
    FrameOutputRegistry registry;
    auto first = std::make_shared<FakeOutputFactory>(StreamProtocol::Udp);
    auto second = std::make_shared<FakeOutputFactory>(StreamProtocol::Udp);
    registry.Register(first);
    registry.Register(second);
    StreamLog log;
    registry.Create(StreamProtocol::Udp, log);
    EXPECT_EQ(first->Created(), 0);
    EXPECT_EQ(second->Created(), 1);
}

TEST(FrameOutputRegistryTest, RefusesNull) {
    FrameOutputRegistry registry;
    EXPECT_THROW(registry.Register(nullptr), std::invalid_argument);
}

TEST(FrameOutputRegistryTest, BundledOutputsCoverEveryProtocolButRtsp) {
    std::shared_ptr<const FrameOutputRegistry> registry = BundledProtocols::Outputs();
    const std::shared_ptr<const StreamProtocolCatalog> protocols = BundledProtocols::Catalog();
    for (const StreamProtocolInfo& info : protocols->All()) {
        EXPECT_EQ(registry->Has(info.id), !info.id.IsRtsp()) << info.id.Id();
    }
}

TEST(FrameOutputRegistryTest, BuiltInProtocolsAreRtspOnly) {
    const std::shared_ptr<const StreamProtocolCatalog> protocols =
        BuiltInStreamProtocols::Catalog();
    ASSERT_EQ(protocols->All().size(), 2u);
    for (const StreamProtocolInfo& info : protocols->All()) {
        EXPECT_TRUE(info.id.IsRtsp()) << info.id.Id();
    }
}

TEST(FrameOutputRegistryTest, ServerStreamsThroughAnInjectedOutput) {
    auto outputs = std::make_shared<FrameOutputRegistry>();
    outputs->Register(std::make_shared<FakeOutputFactory>(StreamProtocol::Udp));
    FrameStreamServer server{SoftwareOnlyEncoderProvider::Registry(), outputs};
    server.SetProtocol(StreamProtocol::Udp);
    server.SetDiscoveryPort(0);
    std::wstring error;
    ASSERT_TRUE(server.Start(0, false, error)) << error;
    EXPECT_EQ(server.Port(), kFakePort);
    server.Stop();
}

TEST(FrameOutputRegistryTest, ServerRefusesAProtocolWithNoOutput) {
    FrameStreamServer server{SoftwareOnlyEncoderProvider::Registry(),
                             std::make_shared<FrameOutputRegistry>()};
    server.SetProtocol(StreamProtocol::Http);
    server.SetDiscoveryPort(0);
    std::wstring error;
    EXPECT_FALSE(server.Start(0, false, error));
    EXPECT_NE(error.find(L"no output for HTTP"), std::wstring::npos) << error;
    EXPECT_FALSE(server.Running());
}

TEST(FrameOutputRegistryTest, ServerRefusesAProtocolThisBuildDoesNotKnow) {
    FrameStreamServer server{SoftwareOnlyEncoderProvider::Registry()};
    server.SetProtocol(StreamProtocol("smoke-signals"));
    server.SetDiscoveryPort(0);
    std::wstring error;
    EXPECT_FALSE(server.Start(0, false, error));
    EXPECT_NE(error.find(L"no output for smoke-signals"), std::wstring::npos) << error;
}

}  // namespace
}  // namespace gazeora
