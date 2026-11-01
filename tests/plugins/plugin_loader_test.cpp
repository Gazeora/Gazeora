// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/services/plugin_loader.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "plugins/fake_plugin_module.h"
#include "plugins/fake_plugin_module_loader.h"

namespace gazeora {
namespace {

void GAZEORA_CALL AppIdFor(void*, const char*, char* appId, size_t capacity) {
    if (capacity > 0) {
        appId[0] = '\0';
    }
}
int32_t GAZEORA_CALL Launch(void*, const char*, char*, size_t) { return GAZEORA_TRUE; }
int32_t GAZEORA_CALL Supports(void*, uint32_t) { return GAZEORA_TRUE; }
int32_t GAZEORA_CALL Create(void*, GazeoraEncoderV1*) { return GAZEORA_FALSE; }

GazeoraGameStoreV1 StoreWithId(const char* id) {
    return {sizeof(GazeoraGameStoreV1), nullptr, id, "Fake", AppIdFor, Launch};
}

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), GAZEORA_PLUGIN_API_VERSION,
                                   "Fake plugin", "2.0"};
const GazeoraEncoderProviderV1 kProvider = {sizeof(GazeoraEncoderProviderV1), nullptr, "Fake",
                                            Supports, Create};

const GazeoraPluginInfoV1* GAZEORA_CALL Info() { return &kInfo; }
const GazeoraPluginInfoV1* GAZEORA_CALL NoInfo() { return nullptr; }

int32_t GAZEORA_CALL RegisterStore(const GazeoraPluginHostV1* host) {
    const GazeoraGameStoreV1 store = StoreWithId("fake");
    return host->register_game_store(host->context, &store);
}

int32_t GAZEORA_CALL RegisterBadIds(const GazeoraPluginHostV1* host) {
    const GazeoraGameStoreV1 upper = StoreWithId("Fake");
    const GazeoraGameStoreV1 reserved = StoreWithId("direct");
    host->register_game_store(host->context, &upper);
    host->register_game_store(host->context, &reserved);
    return host->register_encoder_provider(host->context, &kProvider);
}

int32_t GAZEORA_CALL CreateNoOutput(void*, const GazeoraOutputHostV2*, GazeoraOutputV2*) {
    return GAZEORA_FALSE;
}

// A protocol with an RTSP id and a codec with nothing filled in: both refused.
int32_t GAZEORA_CALL RegisterBadVersion2(const GazeoraPluginHostV1* host) {
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(host);
    if (!v2) {
        return GAZEORA_FALSE;
    }
    const GazeoraProtocolV2 rtsp = {sizeof(GazeoraProtocolV2),
                                    nullptr,
                                    "rtsp-tcp",
                                    "Fake",
                                    "Help",
                                    "/",
                                    GAZEORA_FALSE,
                                    CreateNoOutput};
    GazeoraCodecV2 empty = {};
    empty.struct_size = sizeof(empty);
    empty.id = "fake-codec";
    v2->register_protocol(host->context, &rtsp);
    v2->register_codec(host->context, &empty);
    return GAZEORA_TRUE;
}

int32_t GAZEORA_CALL RegisterProtocol(const GazeoraPluginHostV1* host) {
    const GazeoraProtocolV2 protocol = {sizeof(GazeoraProtocolV2),
                                        nullptr,
                                        "fake-out",
                                        "Fake",
                                        "Help",
                                        "/",
                                        GAZEORA_FALSE,
                                        CreateNoOutput};
    return GazeoraPluginHostV2Of(host)->register_protocol(host->context, &protocol);
}

int32_t GAZEORA_CALL RegisterThenRefuse(const GazeoraPluginHostV1* host) {
    RegisterStore(host);
    return GAZEORA_FALSE;
}

std::shared_ptr<FakeModule> ModuleWith(void* info, void* registerPlugin) {
    return std::make_shared<FakeModule>(std::map<std::string, void*>{
        {GAZEORA_PLUGIN_INFO_SYMBOL, info}, {GAZEORA_PLUGIN_REGISTER_SYMBOL, registerPlugin}});
}

PluginCatalog LoadWith(std::shared_ptr<const IPluginModule> module) {
    FakeModuleLoader modules(std::move(module));
    return PluginLoader(modules).Load({L"C:\\plugins\\fake.dll"});
}

TEST(PluginLoaderTest, ReportsADllThatWontLoad) {
    PluginCatalog catalog = LoadWith(nullptr);
    ASSERT_EQ(catalog.plugins.size(), 1u);
    EXPECT_FALSE(catalog.plugins[0].loaded);
    EXPECT_EQ(catalog.plugins[0].name, L"fake.dll");
    EXPECT_EQ(catalog.plugins[0].error, L"cannot load");
}

TEST(PluginLoaderTest, RefusesADllWithoutTheExports) {
    PluginCatalog catalog = LoadWith(std::make_shared<FakeModule>(std::map<std::string, void*>{}));
    EXPECT_FALSE(catalog.plugins[0].loaded);
    EXPECT_NE(catalog.plugins[0].error.find(L"not a Gazeora plugin"), std::wstring::npos);
}

TEST(PluginLoaderTest, RefusesAPluginWithoutInfo) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(NoInfo), reinterpret_cast<void*>(RegisterStore)));
    EXPECT_FALSE(catalog.plugins[0].loaded);
    EXPECT_TRUE(catalog.gameStores.empty());
}

TEST(PluginLoaderTest, CollectsWhatAPluginRegisters) {
    std::shared_ptr<FakeModule> module =
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterStore));
    std::weak_ptr<FakeModule> alive = module;
    PluginCatalog catalog = LoadWith(std::move(module));
    module.reset();
    ASSERT_EQ(catalog.gameStores.size(), 1u);
    EXPECT_EQ(catalog.gameStores[0]->Id(), "fake");
    const PluginStatus& status = catalog.plugins[0];
    EXPECT_TRUE(status.loaded);
    EXPECT_EQ(status.name, L"Fake plugin");
    EXPECT_EQ(status.version, L"2.0");
    EXPECT_EQ(status.gameStores, 1);
    EXPECT_FALSE(alive.expired());  // the store keeps the DLL loaded
    catalog.gameStores.clear();
    EXPECT_TRUE(alive.expired());
}

TEST(PluginLoaderTest, RefusesInvalidStoreIdsButKeepsTheRest) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterBadIds)));
    const PluginStatus& status = catalog.plugins[0];
    EXPECT_TRUE(status.loaded);
    EXPECT_EQ(status.gameStores, 0);
    EXPECT_EQ(status.encoders, 1);
    EXPECT_NE(status.error.find(L"\"Fake\""), std::wstring::npos) << status.error;
    EXPECT_NE(status.error.find(L"\"direct\""), std::wstring::npos) << status.error;
}

TEST(PluginLoaderTest, DropsEverythingWhenRegisterFails) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterThenRefuse)));
    EXPECT_FALSE(catalog.plugins[0].loaded);
    EXPECT_TRUE(catalog.gameStores.empty());
}

TEST(PluginLoaderTest, RefusesAnRtspProtocolAndAnIncompleteCodec) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterBadVersion2)));
    const PluginStatus& status = catalog.plugins[0];
    EXPECT_TRUE(status.loaded);
    EXPECT_EQ(status.protocols, 0);
    EXPECT_EQ(status.codecs, 0);
    EXPECT_NE(status.error.find(L"\"rtsp-tcp\""), std::wstring::npos) << status.error;
    EXPECT_NE(status.error.find(L"a codec with missing fields"), std::wstring::npos)
        << status.error;
}

TEST(PluginLoaderTest, AProtocolThatMakesNoOutputRefusesToStart) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterProtocol)));
    ASSERT_EQ(catalog.outputs.size(), 1u);
    ASSERT_EQ(catalog.protocols.size(), 1u);
    EXPECT_EQ(catalog.protocols[0].id, StreamProtocol("fake-out"));
    StreamLog log;
    std::unique_ptr<FrameOutput> output = catalog.outputs[0]->Create(log);
    std::wstring error;
    EXPECT_FALSE(output->Start(in_addr{}, 0, error));
    EXPECT_EQ(error, L"Fake couldn't make an output");
}

TEST(PluginLoaderTest, EncoderAPluginCantCreateRefusesToOpen) {
    PluginCatalog catalog = LoadWith(
        ModuleWith(reinterpret_cast<void*>(Info), reinterpret_cast<void*>(RegisterBadIds)));
    ASSERT_EQ(catalog.encoderProviders.size(), 1u);
    std::unique_ptr<VideoEncoder> encoder = catalog.encoderProviders[0]->Create();
    std::wstring error;
    EXPECT_FALSE(encoder->Open(VideoEncoder::Settings{}, error));
    EXPECT_EQ(error, L"Fake could not create an encoder");
}

}  // namespace
}  // namespace gazeora
