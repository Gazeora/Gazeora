// The stream's protocol work, per frame or per message: RTP packetization of both codecs and
// the receiver's depacketization, parameter sets, RTCP reports, RTSP requests and responses,
// and the bitrate controller.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bench_util.h"
#include "gazeora/host/stream/bitrate_controller.h"
#include "gazeora/host/stream/rtcp.h"
#include "gazeora/host/stream/rtp_video.h"
#include "gazeora/host/stream/rtsp_message.h"

namespace gazeora::bench {
namespace {

// Typical sizes at 8 Mbit/s and 30 fps: a frame is about 33 KB, a key frame several times that.
constexpr size_t kFrameBytes = 33 * 1024;
constexpr size_t kKeyFrameBytes = 150 * 1024;
constexpr size_t kMaxPacket = 1400;

// A NAL unit: `header`, then `size` bytes of payload with no zero bytes, so no start code
// shows up inside it.
std::string Nal(const std::string& header, size_t size) {
    std::string nal = header;
    uint32_t seed = uint32_t(size);
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
        nal += char(b ? b : 1);
    }
    return nal;
}

struct AccessUnits {
    std::vector<std::string> frame;     // one slice
    std::vector<std::string> keyFrame;  // parameter sets and an IDR slice
};

AccessUnits UnitsFor(VideoCodec codec) {
    AccessUnits units;
    if (codec == VideoCodec::H264) {
        units.frame = {Nal("\x41", kFrameBytes)};
        units.keyFrame = {Nal("\x67\x42\xC0\x28", 12), Nal("\x68\xCE\x3C\x80", 0),
                          Nal("\x65", kKeyFrameBytes)};
    } else {
        // Two-byte headers: type << 1, then layer 0, temporal id 1.
        units.frame = {Nal(std::string("\x02\x01", 2), kFrameBytes)};
        units.keyFrame = {Nal(std::string("\x40\x01", 2), 20), Nal(std::string("\x42\x01", 2), 40),
                          Nal(std::string("\x44\x01", 2), 6),
                          Nal(std::string("\x26\x01", 2), kKeyFrameBytes)};
    }
    return units;
}

std::string ToAnnexB(const std::vector<std::string>& nals) {
    std::string annexB;
    for (const std::string& nal : nals) annexB += std::string("\0\0\0\1", 4) + nal;
    return annexB;
}

TEST(Rtp, PacketizeAndDepacketize) {
    PrintHeader("RTP video, 1400-byte packets");
    for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) {
        const RtpPayloadFormat& format = RtpPayloadFormat::For(codec);
        AccessUnits units = UnitsFor(codec);
        struct Case {
            const char* name;
            const std::vector<std::string>* nals;
        };
        for (Case c : {Case{"frame 33 KB", &units.frame}, Case{"key frame 150 KB", &units.keyFrame}}) {
            std::string prefix = std::string(CodecName(codec)) + " " + c.name + ": ";
            std::string annexB = ToAnnexB(*c.nals);
            std::vector<std::string> nals, packets;
            Stats split = Measure(200, [&] {
                SplitAnnexB(annexB, nals);
                Consume(nals.data());
            });
            Print(prefix + "SplitAnnexB", split, Throughput(double(annexB.size()), split.median));

            uint16_t seq = 0;
            Stats pack = Measure(200, [&] {
                format.Packetize(nals, 0x12345678, 0x1234, kMaxPacket, seq, packets);
                Consume(packets.data());
            });
            Print(prefix + "Packetize", pack, std::to_string(packets.size()) + " packets");

            RtpVideoDepacketizer depacketizer(codec);
            std::string rebuilt;
            Stats unpack = Measure(200, [&] {
                rebuilt.clear();
                for (const std::string& p : packets) {
                    depacketizer.Add(reinterpret_cast<const uint8_t*>(p.data()) + 12, p.size() - 12,
                                     rebuilt);
                }
                Consume(rebuilt.data());
            });
            EXPECT_EQ(rebuilt.size(), annexB.size()) << prefix;
            Print(prefix + "Depacketize (player side)", unpack);
        }
    }
}

TEST(Rtp, ParameterSets) {
    PrintHeader("ParameterSetCache, per key frame");
    for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) {
        const RtpPayloadFormat& format = RtpPayloadFormat::For(codec);
        AccessUnits units = UnitsFor(codec);
        ParameterSetCache cache(format);
        Print(std::string(CodecName(codec)) + " Update", Measure(200, [&] {
                  Consume(&cache);
                  cache.Update(units.keyFrame);
              }, 3, 100));
        // A key frame without its parameter sets, as some encoders send them once only.
        std::vector<std::string> idrOnly = {units.keyFrame.back().substr(0, 64)};
        std::vector<std::string> nals;
        Print(std::string(CodecName(codec)) + " PrependTo", Measure(200, [&] {
                  nals = idrOnly;
                  cache.PrependTo(nals);
                  Consume(nals.data());
              }, 3, 100));
        std::string fmtp;
        Print(std::string(CodecName(codec)) + " Fmtp (SDP)", Measure(200, [&] {
                  fmtp = cache.Fmtp();
                  Consume(fmtp.data());
              }, 3, 100));
    }
}

TEST(Rtcp, ReportsAndClock) {
    PrintHeader("RTCP");
    std::string report;
    Print("BuildSenderReport", Measure(200, [&] {
              report = BuildSenderReport(0x1234, 1000, 1400000);
              Consume(report.data());
          }, 3, 100));
    // A receiver report with one block, as VLC sends every few seconds.
    const uint8_t rr[32] = {0x81, 201, 0, 7,    0xAA, 0xBB, 0xCC, 0xDD,  // header, reporter
                            0x00, 0x00, 0x12, 0x34,                       // source
                            8,    0,    0,    42,                         // lost: 3%, 42 total
                            0,    1,    0,    0,    0, 0, 0, 30,          // highest seq, jitter
                            0,    0,    0,    0,    0, 0, 0, 0};          // LSR, DLSR
    std::vector<ReceptionReport> blocks;
    Print("ParseReceiverReports", Measure(200, [&] {
              blocks = ParseReceiverReports(rr, sizeof(rr));
              Consume(blocks.data());
          }, 3, 100));
    EXPECT_EQ(blocks.size(), 1u);
    uint64_t clock = 0;
    Print("RtpClock64", Measure(200, [&] {
              clock += RtpClock64();
              Consume(&clock);
          }, 3, 100));
}

TEST(Rtsp, MessagesFromAPlayer) {
    PrintHeader("RTSP, per message");
    const std::string setup =
        "SETUP rtsp://192.168.1.20:8554/track1 RTSP/1.0\r\n"
        "CSeq: 4\r\n"
        "User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)\r\n"
        "Transport: RTP/AVP;unicast;client_port=50000-50001\r\n\r\n";
    RtspRequest request;
    Print("RtspRequest::Parse (SETUP)", Measure(200, [&] {
              request = RtspRequest();
              EXPECT_TRUE(RtspRequest::Parse(setup, request));
          }, 3, 50));
    TransportSpec transport;
    Print("Header + TransportSpec::Parse", Measure(200, [&] {
              EXPECT_TRUE(TransportSpec::Parse(request.Header("transport"), transport));
          }, 3, 50));
    const std::string sdp =
        "v=0\r\no=- 0 0 IN IP4 192.168.1.20\r\ns=Gazeora\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\n"
        "m=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;sprop-parameter-sets=Z0LAKA==,aM48gA==\r\n"
        "a=control:track1\r\n";
    std::string response;
    Print("RtspResponse (DESCRIBE, with SDP)", Measure(200, [&] {
              response = RtspResponse("200 OK", request)
                             .Header("Content-Base", "rtsp://192.168.1.20:8554/")
                             .Body("application/sdp", sdp)
                             .Serialize();
              Consume(response.data());
          }, 3, 50));

    // What arrives on a connection playing over TCP: interleaved RTCP reports and keep-alives.
    std::string interleaved = std::string("$\x01\x00\x20", 4) + std::string(32, '\x81');
    std::string keepalive =
        "GET_PARAMETER rtsp://192.168.1.20:8554/ RTSP/1.0\r\nCSeq: 9\r\nSession: 1A2B3C\r\n\r\n";
    std::string chunk = interleaved + keepalive + interleaved;
    std::string text;
    Print("RtspReader (2 RTCP frames + 1 request)", Measure(200, [&] {
              RtspReader reader;
              reader.Append(chunk.data(), chunk.size());
              int items = 0;
              while (reader.Next(text) != RtspReader::Item::None) ++items;
              EXPECT_EQ(items, 3);
          }, 3, 50));
}

TEST(Bitrate, ControllerPerFrame) {
    PrintHeader("BitrateController");
    BitrateController controller;
    uint64_t now = 0;
    Print("Report + Update, 4 players", Measure(200, [&] {
              now += 33;
              for (uint64_t player = 0; player < 4; ++player) {
                  controller.Report(player, player == 3 ? 0.05f : 0.0f, now);
              }
              controller.Update(now, 4);
          }, 3, 100));
}

}  // namespace
}  // namespace gazeora::bench
