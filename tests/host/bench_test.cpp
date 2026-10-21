// TEMPORARY benchmark: per-frame CPU cost of the stream path at 1080p.
#include <windows.h>
#include <gtest/gtest.h>
#include <mfapi.h>

#include <chrono>
#include <cstdio>
#include <vector>

#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/host/stream/rtp_video.h"

using namespace gazeora;
using Clock = std::chrono::steady_clock;

static double Ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

TEST(Bench, StreamPath) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
    const uint32_t W = 1920, H = 1080;
    std::vector<uint8_t> bgra(size_t(W) * H * 4), nv12;
    uint32_t seed = 1;
    for (auto& b : bgra) b = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
    BgraToNv12(bgra.data(), W, W, H, nv12);
    auto t0 = Clock::now();
    for (int i = 0; i < 50; ++i) BgraToNv12(bgra.data(), W, W, H, nv12);
    auto t1 = Clock::now();
    printf("BgraToNv12 1080p: %.2f ms\n", Ms(t0, t1) / 50);

    for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) {
        VideoEncoder encoder;
        std::wstring error;
        ASSERT_TRUE(encoder.Open(codec, W, H, 30, 8000000, 30, true, error));
        std::vector<VideoEncoder::Frame> frames;
        double encodeMs = 0, packMs = 0;
        uint16_t seq = 0;
        std::vector<std::string> nals, packets;
        for (int i = 0; i < 60; ++i) {
            bgra[i * 4097 % bgra.size()] ^= 0xFF;
            BgraToNv12(bgra.data(), W, W, H, nv12);
            frames.clear();
            auto a = Clock::now();
            encoder.Encode(nv12.data(), int64_t(i) * 333333, frames, error);
            auto b = Clock::now();
            for (auto& f : frames) {
                SplitAnnexB(f.annexB, nals);
                PacketizeRtpVideo(codec, nals, 0, 0, 1400, seq, packets);
            }
            auto c = Clock::now();
            if (i >= 10) encodeMs += Ms(a, b), packMs += Ms(b, c);
        }
        printf("%s encode call: %.2f ms, split+packetize: %.3f ms\n", CodecName(codec),
               encodeMs / 50, packMs / 50);
    }
}
