// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/packet_batch.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace gazeora {
namespace {

TEST(PacketBatchTest, KeepsEachPacketAsItWas) {
    PacketBatch batch;
    batch.Assign({"first", std::string("\0two", 4), "", "fourth packet"});
    ASSERT_EQ(batch.Count(), 4u);
    EXPECT_EQ(batch.Packet(0), "first");
    EXPECT_EQ(batch.Packet(1), std::string_view("\0two", 4));
    EXPECT_EQ(batch.Packet(2), "");
    EXPECT_EQ(batch.Packet(3), "fourth packet");
}

TEST(PacketBatchTest, TakesAnotherFrameInTheSameBuffer) {
    PacketBatch batch;
    batch.Assign({std::string(1400, 'a'), std::string(1400, 'b')});
    const char* buffer = batch.Packet(0).data();
    batch.Assign({std::string(700, 'c')});
    ASSERT_EQ(batch.Count(), 1u);
    EXPECT_EQ(batch.Packet(0), std::string(700, 'c'));
    EXPECT_EQ(batch.Packet(0).data(), buffer);  // no new allocation for a smaller frame
    batch.Assign({});
    EXPECT_EQ(batch.Count(), 0u);
}

}  // namespace
}  // namespace gazeora
