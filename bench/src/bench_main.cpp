// gazeora_bench: how long each part of Gazeora takes, from the hook's capture in the game to
// RTP packets on the network. One gtest case per area; pick some with --gtest_filter, e.g.
// --gtest_filter=Rtp.*:Rtsp.*. Prints its numbers; it only fails when something it measures
// doesn't work at all. Numbers mean something in a Release build only.
#include <windows.h>
#include <gtest/gtest.h>
#include <mfapi.h>

#include <cstdio>

namespace {

class BenchEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
#ifndef NDEBUG
        printf("*** Debug build: these numbers say little. Build Release to benchmark. ***\n");
#endif
        // What the stream server's encode thread runs with.
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION, MFSTARTUP_LITE);
    }
    void TearDown() override {
        MFShutdown();
        CoUninitialize();
    }
};

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BenchEnvironment);
    return RUN_ALL_TESTS();
}
