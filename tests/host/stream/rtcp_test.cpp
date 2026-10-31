// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/host/stream/rtcp.h"

namespace gazeora {
namespace {

uint32_t Get32(const std::string& data, size_t at) {
    return uint32_t(uint8_t(data[at])) << 24 | uint8_t(data[at + 1]) << 16 |
           uint8_t(data[at + 2]) << 8 | uint8_t(data[at + 3]);
}

TEST(RtcpTest, SenderReportCarriesCountsAndCname) {
    std::string report = BuildSenderReport(0xCAFEF00D, 10, 12345);
    // Each packet exactly as long as its length field says: SR 7 words, SDES 5.
    ASSERT_EQ(report.size(), 48u);
    EXPECT_EQ(Get32(report, 0), 0x80C80006u);
    EXPECT_EQ(Get32(report, 4), 0xCAFEF00Du);
    EXPECT_EQ(Get32(report, 20), 10u);
    EXPECT_EQ(Get32(report, 24), 12345u);
    EXPECT_EQ(Get32(report, 28), 0x81CA0004u);
    EXPECT_EQ(report.substr(38, 7), "Gazeora");
}

TEST(RtcpTest, ParsesReceiverReportBlocksInACompoundPacket) {
    // RR with one block, then an SDES the parser has to step over.
    const uint8_t packet[] = {
        0x81, 201,  0x00, 0x07, 0x11, 0x22, 0x33, 0x44,  // RR, 1 block, 8 words, reporter
        0xCA, 0xFE, 0xF0, 0x0D,                          // source
        64,   0xFF, 0xFF, 0xFF,                          // 25% lost, cumulative -1
        0x00, 0x00, 0x10, 0x00,                          // highest sequence
        0x00, 0x00, 0x01, 0x68,                          // jitter 360
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LSR, DLSR
        0x81, 202,  0x00, 0x01, 0x11, 0x22, 0x33, 0x44,  // SDES, 1 word
    };
    std::vector<ReceptionReport> reports = ParseReceiverReports(packet, sizeof(packet));
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].source, 0xCAFEF00Du);
    EXPECT_FLOAT_EQ(reports[0].fractionLost, 0.25f);
    EXPECT_EQ(reports[0].cumulativeLost, -1);
    EXPECT_EQ(reports[0].jitter, 360u);
}

TEST(RtcpTest, FindsKeyFrameRequests) {
    // RR without blocks, then a PLI for 0xCAFEF00D.
    const uint8_t pli[] = {
        0x80, 201,  0x00, 0x01, 0x11, 0x22, 0x33, 0x44,  // RR, no blocks
        0x81, 206,  0x00, 0x02, 0x11, 0x22, 0x33, 0x44,  // PSFB, PLI, sender
        0xCA, 0xFE, 0xF0, 0x0D,                          // media source
    };
    EXPECT_TRUE(AsksForKeyFrame(pli, sizeof(pli), 0xCAFEF00Du));
    EXPECT_FALSE(AsksForKeyFrame(pli, sizeof(pli), 0x12345678u));  // another stream's
    EXPECT_FALSE(AsksForKeyFrame(pli, sizeof(pli) - 4, 0xCAFEF00Du));

    const uint8_t fir[] = {
        0x84, 206,  0x00, 0x04, 0x11, 0x22, 0x33, 0x44,  // PSFB, FIR, sender
        0x00, 0x00, 0x00, 0x00, 0xCA, 0xFE, 0xF0, 0x0D,  // media 0, then the entry's SSRC
        0x01, 0x00, 0x00, 0x00,                          // sequence number
    };
    EXPECT_TRUE(AsksForKeyFrame(fir, sizeof(fir), 0xCAFEF00Du));

    const uint8_t receiverReport[] = {0x80, 201,  0x00, 0x01, 0x11, 0x22,
                                      0x33, 0x44, 0xCA, 0xFE, 0xF0, 0x0D};
    EXPECT_FALSE(AsksForKeyFrame(receiverReport, sizeof(receiverReport), 0xCAFEF00Du));
}

TEST(RtcpTest, IgnoresTruncatedBlocks) {
    const uint8_t packet[] = {0x81, 201, 0x00, 0x07, 0x11, 0x22, 0x33, 0x44, 0xCA, 0xFE};
    EXPECT_TRUE(ParseReceiverReports(packet, sizeof(packet)).empty());
}

}  // namespace
}  // namespace gazeora
