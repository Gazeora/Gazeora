// Video encoding: each codec on the GPU's encoder and on the software one, and the whole
// VideoPipeline (NV12 conversion, encoding, packetization) the stream server runs per frame.
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "bench_util.h"
#include "gazeora/host/encode/color_convert.h"
#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/host/stream/video_pipeline.h"

namespace gazeora::bench {
namespace {

constexpr int kFrames = 150;
constexpr int kWarmupFrames = 10;
constexpr uint32_t kBitrate = 8000000;
constexpr int64_t kFrameTime = 333333;  // 30 fps, in 100 ns units

// Distinct frames to cycle through, converted ahead so only the encoder is timed.
std::vector<std::vector<uint8_t>> Nv12Frames(Size size, int count) {
    std::vector<std::vector<uint8_t>> frames{size_t(count)};
    std::vector<uint8_t> bgra;
    for (int i = 0; i < count; ++i) {
        FillScene(bgra, size.width, size.height, i);
        BgraToNv12(bgra.data(), size.width, size.width, size.height, frames[size_t(i)]);
    }
    return frames;
}

std::string Kilobytes(double bytes) {
    char text[32];
    snprintf(text, sizeof(text), "%.1f KB", bytes / 1024);
    return text;
}

// Encodes kFrames frames and prints the time per Encode call, and the latency from handing
// a frame in to getting it back (hardware encoders may return a frame a call or two later).
void BenchEncoder(VideoCodec codec, bool preferHardware, Size size,
                  const std::vector<std::vector<uint8_t>>& frames) {
    std::string label = std::string(CodecName(codec)) + (preferHardware ? " hw " : " sw ") +
                        size.name;
    auto encoder = CreateVideoEncoder();
    VideoEncoder::Settings settings;
    settings.codec = codec;
    settings.width = size.width;
    settings.height = size.height;
    settings.bitrate = kBitrate;
    settings.preferHardware = preferHardware;
    std::wstring error;
    double openStart = QpcSeconds();
    bool opened = encoder->Open(settings, error);
    std::string opening = Narrow(encoder->Name()) + ", opened in " +
                          FormatSeconds(QpcSeconds() - openStart);
    if (!opened) {
        PrintNote(label, "could not open: " + Narrow(error));
        return;
    }
    if (preferHardware && !encoder->IsHardware()) {
        PrintNote(label, "no hardware encoder for this codec");
        return;
    }

    std::map<int64_t, double> submittedAt;  // by sample time
    std::vector<double> calls, keyLatency, latency;
    double keyBytes = 0, bytes = 0;
    int keyFrames = 0, outFrames = 0;
    std::vector<VideoEncoder::Frame> out;
    for (int i = 0; i < kFrames; ++i) {
        int64_t time = i * kFrameTime;
        out.clear();
        double start = QpcSeconds();
        submittedAt[time] = start;
        bool ok = encoder->Encode(frames[size_t(i) % frames.size()].data(), time, out, error);
        double end = QpcSeconds();
        ASSERT_TRUE(ok) << Narrow(error);
        if (i >= kWarmupFrames) calls.push_back(end - start);
        for (const VideoEncoder::Frame& frame : out) {
            auto at = submittedAt.find(frame.time);
            if (at == submittedAt.end() || frame.time < kWarmupFrames * kFrameTime) continue;
            (frame.keyFrame ? keyLatency : latency).push_back(end - at->second);
            if (frame.keyFrame) {
                keyBytes += double(frame.annexB.size());
                ++keyFrames;
            } else {
                bytes += double(frame.annexB.size());
                ++outFrames;
            }
        }
    }
#if defined(__GNUC__) && defined(__x86_64__)
    // HEVCVideoExtensionEncoder returns with the FPU in MMX state, and MinGW's printf formats
    // floats with x87, so the next one would come out as "nan". EMMS gives x87 back.
    asm volatile("emms");
#endif
    PrintNote(label, opening);
    Print("  Encode call", Stats::Of(calls));
    Print("  in to out, frame", Stats::Of(latency),
          outFrames ? Kilobytes(bytes / outFrames) + " each" : "");
    Print("  in to out, key frame", Stats::Of(keyLatency),
          keyFrames ? Kilobytes(keyBytes / keyFrames) + " each" : "");
}

TEST(Encode, EveryCodecAndEncoder) {
    PrintHeader("VideoEncoder, 8 Mbit/s, 30 fps, key frame every 30");
    for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) {
        if (!VideoEncoder::Available(codec)) {
            PrintNote(CodecName(codec), "no encoder on this PC");
            continue;
        }
        for (Size size : {k720p, k1080p}) {
            auto frames = Nv12Frames(size, 16);
            for (bool hardware : {true, false}) {
                BenchEncoder(codec, hardware, size, frames);
            }
        }
    }
}

TEST(Encode, VideoPipelinePerFrame) {
    PrintHeader("VideoPipeline::Encode, BGRA8 to RTP packets");
    const Size size = k1080p;
    std::vector<Picture> pictures(16);
    for (size_t i = 0; i < pictures.size(); ++i) {
        FillScene(pictures[i].bgra, size.width, size.height, int(i));
        pictures[i].width = size.width;
        pictures[i].height = size.height;
    }
    for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) {
        if (!VideoEncoder::Available(codec)) continue;
        VideoPipeline pipeline;
        VideoPipeline::Config config;
        config.codec = codec;
        config.bitrate = kBitrate;
        config.ssrc = 0x1234;
        bool reopened = false;
        std::wstring error;
        ASSERT_TRUE(pipeline.Prepare(config, size.width, size.height, reopened, error))
            << Narrow(error);
        size_t packets = 0, frames = 0;
        auto sink = [&](const VideoPipeline::EncodedFrame& frame) {
            packets += frame.packets.size();
            ++frames;
        };
        int i = 0;
        Stats s = Measure(kFrames, [&] {
            Picture& picture = pictures[size_t(i) % pictures.size()];
            picture.clock = uint64_t(i++) * 3000;  // 30 fps on the 90 kHz clock
            bool fmtpChanged = false;
            EXPECT_TRUE(pipeline.Encode(picture, sink, fmtpChanged, error)) << Narrow(error);
        }, kWarmupFrames);
        char note[96];
        snprintf(note, sizeof(note), "%.1f packets/frame, %s", frames ? double(packets) / frames : 0.0,
                 Narrow(pipeline.EncoderName()).c_str());
        Print(std::string(CodecName(codec)) + " " + size.name, s, note);
    }
}

}  // namespace
}  // namespace gazeora::bench
