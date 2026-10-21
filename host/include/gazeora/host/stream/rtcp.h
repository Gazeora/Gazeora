#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gazeora {

// The 90 kHz media clock RTP timestamps run on, from QueryPerformanceCounter. The full 64-bit
// value goes to the encoder, as sample times must not wrap; RTP keeps the low 32 bits.
uint64_t RtpClock64();

// RTCP (RFC 3550), the parts the stream uses.

// A sender report plus the SDES CNAME every compound packet needs (6.4.1, 6.5). It ties RTP
// timestamps to wall-clock time, so players time frames by when they were captured rather
// than guessing from arrival, and don't buffer extra to be safe.
std::string BuildSenderReport(uint32_t ssrc, uint32_t packetsSent, uint32_t octetsSent);

// One report block of a receiver report (6.4.2): how a player is receiving one source.
struct ReceptionReport {
    uint32_t source = 0;
    float fractionLost = 0;    // since the player's previous report, 0..1
    int32_t cumulativeLost = 0;  // signed: duplicates can make it negative
    uint32_t jitter = 0;       // in RTP timestamp units
};

// The report blocks of every receiver report in a compound RTCP packet.
std::vector<ReceptionReport> ParseReceiverReports(const uint8_t* packet, size_t size);

}  // namespace gazeora
