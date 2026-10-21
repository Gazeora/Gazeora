#include "gazeora/host/stream/rtcp.h"

#include <windows.h>

namespace gazeora {

namespace {
constexpr uint64_t kSeconds1601To1900 = 9435484800ull;
constexpr uint8_t kReceiverReport = 201;

uint32_t Get32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
}  // namespace

uint64_t RtpClock64() {
    static const LONGLONG freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return uint64_t(now.QuadPart / freq * 90000 + now.QuadPart % freq * 90000 / freq);
}

std::string BuildSenderReport(uint32_t ssrc, uint32_t packetsSent, uint32_t octetsSent) {
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    uint32_t rtpNow = uint32_t(RtpClock64());
    uint64_t ticks = uint64_t(now.dwHighDateTime) << 32 | now.dwLowDateTime;  // 100 ns
    uint32_t ntpSeconds = uint32_t(ticks / 10000000 - kSeconds1601To1900);
    uint32_t ntpFraction = uint32_t((ticks % 10000000 << 32) / 10000000);

    std::string report;
    report.reserve(44);
    auto put32 = [&](uint32_t v) {
        for (int shift = 24; shift >= 0; shift -= 8) report += char(v >> shift);
    };
    put32(0x80C80006);  // V=2, no report blocks, SR, 7 words
    put32(ssrc);
    put32(ntpSeconds);
    put32(ntpFraction);
    put32(rtpNow);
    put32(packetsSent);
    put32(octetsSent);
    put32(0x81CA0004);  // V=2, one chunk, SDES, 5 words
    put32(ssrc);
    report +=
        "\x01\x07"
        "Gazeora";           // CNAME
    report.append(2, '\0');  // end of items, padded to a word
    return report;
}

std::vector<ReceptionReport> ParseReceiverReports(const uint8_t* packet, size_t size) {
    std::vector<ReceptionReport> reports;
    // A compound packet: RTCP packets back to back, each with its length in words.
    for (size_t at = 0; at + 8 <= size;) {
        size_t length = ((size_t(packet[at + 2]) << 8 | packet[at + 3]) + 1) * 4;
        size_t blocks = packet[at] & 0x1F;
        if (packet[at + 1] == kReceiverReport) {
            for (size_t block = 0; block < blocks && at + 8 + (block + 1) * 24 <= size; ++block) {
                const uint8_t* p = packet + at + 8 + block * 24;
                ReceptionReport report;
                report.source = Get32(p);
                report.fractionLost = p[4] / 256.0f;
                report.cumulativeLost = int32_t(uint32_t(p[5]) << 24 | p[6] << 16 | p[7] << 8) >> 8;
                report.jitter = Get32(p + 12);
                reports.push_back(report);
            }
        }
        at += length;
    }
    return reports;
}

}  // namespace gazeora
