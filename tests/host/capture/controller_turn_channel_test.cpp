// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/capture/controller_turn_channel.h"

#include <gtest/gtest.h>

namespace gazeora {
namespace {

// The shared block a capture client makes for a game, here for this process.
class SharedBlock {
public:
    SharedBlock() {
        m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                       sizeof(SharedHeader),
                                       SharedMemoryName(GetCurrentProcessId()).c_str());
        m_shared = static_cast<SharedHeader*>(
            MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)));
        m_shared->magic = kSharedMagic;
        m_shared->version = kSharedVersion;
    }
    ~SharedBlock() {
        UnmapViewOfFile(m_shared);
        CloseHandle(m_mapping);
    }
    SharedHeader* operator->() { return m_shared; }

private:
    HANDLE m_mapping;
    SharedHeader* m_shared;
};

LONGLONG Now() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

TEST(ControllerTurnChannelTest, NothingWithoutTheGamesBlock) {
    ControllerTurnChannel channel;
    channel.Attach(GetCurrentProcessId());
    EXPECT_FALSE(channel.Attached());
    channel.AddTurn(5.0, 5.0);  // goes nowhere, and doesn't crash
    EXPECT_FALSE(channel.Read().recent);
}

TEST(ControllerTurnChannelTest, AddsTurnsAndWritesTheStick) {
    SharedBlock block;
    ControllerTurnChannel channel;
    channel.SetStick(200.0f, 0.15f);  // before attaching: written when it does
    channel.Attach(GetCurrentProcessId());
    ASSERT_TRUE(channel.Attached());
    EXPECT_EQ(block->stickDegreesPerSecond, 200);
    EXPECT_EQ(block->stickDeadZonePermille, 150);
    channel.AddTurn(1.5, -0.25);
    channel.AddTurn(0.5, 0.0);
    EXPECT_EQ(block->headYawMicro, 2000000);
    EXPECT_EQ(block->headPitchMicro, -250000);
    channel.Attach(0);
    EXPECT_FALSE(channel.Attached());
}

TEST(ControllerTurnChannelTest, ReadsWhatTheHookSaw) {
    SharedBlock block;
    ControllerTurnChannel channel;
    channel.Attach(GetCurrentProcessId());
    EXPECT_FALSE(channel.Read().recent);
    block->controllerReadQpc = Now();
    block->controllerActiveQpc = 1234;
    block->controllerPaths = kPathSonyRead | kPathXInput;
    ControllerTurnChannel::Reads reads = channel.Read();
    EXPECT_TRUE(reads.recent);
    EXPECT_EQ(reads.activeQpc, 1234);
    EXPECT_EQ(reads.paths, kPathSonyRead | kPathXInput);
}

TEST(ControllerTurnChannelTest, LeavesABlockOfAnotherVersion) {
    SharedBlock block;
    block->version = kSharedVersion - 1;
    ControllerTurnChannel channel;
    channel.Attach(GetCurrentProcessId());
    EXPECT_FALSE(channel.Attached());
}

}  // namespace
}  // namespace gazeora
